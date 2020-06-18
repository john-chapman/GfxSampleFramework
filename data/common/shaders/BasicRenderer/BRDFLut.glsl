#include "shaders/def.glsl"
#include "shaders/Sampling.glsl"

uniform writeonly image2D txBRDFLut;

vec3 HemisphereGGX(in vec2 _xi, in float _a, in vec3 _N)
{
	_a = _a *_a;
	float phi = 2.0 * kPi * _xi.x;
	float ct = sqrt((1.0 - _xi.y) / (1.0 + (_a * _a - 1.0) * _xi.y));
	float st = sqrt(1.0 - ct * ct);
	vec3 H;
	H.x = st * cos(phi);
	H.y = st * sin(phi);
	H.z = ct;
	vec3 up = abs(_N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
	vec3 tx = normalize(cross(up, _N));
	vec3 ty = cross(_N, tx);
	return tx * H.x + ty * H.y + _N * H.z;
}

void ImportanceSampleCosDir(
	in vec2   u,
	in vec3   N,
	out vec3  L,
	out float NoL,
	out float pdf)
{
	vec3 up = abs(N.z) < 0.999 ? vec3 (0, 0, 1) : vec3 (1, 0, 0);
	vec3 tangentX = normalize(cross(up, N));
	vec3 tangentY = cross(N, tangentX);

	float r   = sqrt(u.x);
	float phi = u.y * kPi * 2;

	L = vec3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - u.x )));
	L = normalize(tangentX * L.y + tangentY * L .x + N * L.z);

	NoL = dot(L, N);
	pdf = NoL / kPi;
}

float G_SchlickBeckmann(in float _NdotV, in float _k)
{
	return _NdotV / (_NdotV * (1.0 - _k) + _k);
}

float G_Smith_SchlickBeckmann(in float _NdotV, in float _NdotL, in float _k)
{
	return G_SchlickBeckmann(_NdotV, _k) * G_SchlickBeckmann(_NdotL, _k);
}

float Diffuse_F(in float _m, in float _fd90)
{
	return 1.0 + _fd90 * pow(2.0, (-5.55473 * _m -6.98316) * _m); // SG approximation of the line above
}

float Diffuse_Burley(in float _NdotV, in float _NdotL, in float _VdotH, in float _a)
{
#if 0
 // original Disney diffuse, does not conserve energy
	float fd90 = 0.5 + 2.0 * (_VdotH * _VdotH) * _a - 1.0;
	return Diffuse_F(_NdotV, fd90) * Lighting_Diffuse_F(_NdotL, fd90);
#else	
 // Lagarde/Rousiers renormalization
	float scale = mix(1.0, 1.0/1.51, _a);
	float bias = mix(0.0, 0.5, _a);
	float fd90 = bias + 2.0 * (_VdotH * _VdotH) * _a - 1.0;
	return Diffuse_F(_NdotV, fd90) * Diffuse_F(_NdotL, fd90) * scale;
#endif
}


vec3 IntegrateBRDF(in float _NoV, in float _roughness)
{
	const vec3 N = vec3(0.0, 0.0, 1.0);
	const vec3 V = vec3(sqrt(1.0 - _NoV * _NoV), 0.0, _NoV);

	vec3 ret = vec3(0.0);
	const int sampleCount = 1024;
	float rn = 1.0 / float(sampleCount);
	for (int i = 0; i < sampleCount; ++i)
	{
		vec2  xi  = Sampling_Hammersley2d(i, rn);
		vec3  H   = HemisphereGGX(xi, _roughness, N);
		float VoH = saturate(dot(V, H));
		vec3  L   = normalize(2.0 * VoH * H - V);
		float NoL = saturate(L.z);
		float NoH = saturate(H.z);

	 // specular GGX
		if (NoL > 0.0)
		{
			float G    = G_Smith_SchlickBeckmann(_NoV, NoL, (_roughness * _roughness) * 0.5);
			float Vis  = (G * VoH) / (NoH * _NoV);
			float Fc   = pow(1.0 - VoH, 5.0);
			ret.x     += (1.0 - Fc) * Vis;
			ret.y     += Fc * Vis;
		}

	 // diffuse GGX
		xi = fract(xi + 0.5);
		float pdf;
		ImportanceSampleCosDir(xi, N, L, NoL, pdf);
		if ( NoL >0)
		{
			float LdotH  = saturate(dot(L, normalize(V + L)));
			float NdotV  = saturate(dot(N, V));
			ret.z       += Diffuse_Burley(NdotV, NoL, LdotH, sqrt(_roughness));
		}
	}

	return ret * rn;
}

void main()
{
	ivec2 txSize = ivec2(imageSize(txBRDFLut).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
	{
		return;
	}

	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize);
	uv += vec2(0.5) / vec2(txSize);
	vec3 ret = IntegrateBRDF(uv.x, uv.y);
	imageStore(txBRDFLut, ivec2(gl_GlobalInvocationID.xy), vec4(ret, 1.0));
}
