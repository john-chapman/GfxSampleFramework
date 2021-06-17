#include "shaders/def.glsl"
#include "shaders/Envmap.glsl"

noperspective in vec2 vUv;
noperspective in vec4 vColor;

uniform vec2  uScaleUv;
uniform vec2  uBiasUv;
uniform float uLayer;
uniform float uMip;
uniform uvec4 uRgbaMask;
uniform int   uIsDepth;

#if   defined(TEXTURE_1D)
	#define TextureType sampler1D
#elif defined(TEXTURE_1D_ARRAY)
	#define TextureType sampler1DArray
#elif defined(TEXTURE_2D)
	#define TextureType sampler2D
#elif defined(TEXTURE_2D_ARRAY)
	#define TextureType sampler2DArray
#elif defined(TEXTURE_3D)
	#define TextureType sampler3D
#elif defined(TEXTURE_CUBE_MAP)
	#define TextureType samplerCube
#elif defined(TEXTURE_CUBE_MAP_ARRAY)
	#define TextureType samplerCubeArray
#else
	//#error TextureVis_fs: No texture type defined.
#endif

#ifdef TextureType
	uniform TextureType txTexture;
#endif

uniform sampler2D txRadar;

layout(location=0) out vec4 fResult;

void main() 
{
	vec4 ret;
	vec2 texcoord = vUv;//vec2(vUv.x, 1.0 - vUv.y);
	#if   defined(TEXTURE_1D)
		ret = textureLod(txTexture, vUv.x * uScaleUv.x + uBiasUv.x, uMip);
		
	#elif defined(TEXTURE_1D_ARRAY)
		vec2 uv;
		uv.x = texcoord.x * uScaleUv.x + uBiasUv.x;
		uv.y = uLayer;
		ret = textureLod(txTexture, uv, uMip);
		
	#elif defined(TEXTURE_2D)
		ret = textureLod(txTexture, texcoord * uScaleUv + uBiasUv, uMip);

	#elif defined(TEXTURE_2D_ARRAY)
		vec3 uvw;
		uvw.xy = texcoord * uScaleUv + uBiasUv;
		uvw.z  = uLayer;
		ret = textureLod(txTexture, uvw, uMip);
		
	#elif defined(TEXTURE_3D)
		vec3 uvw;
		uvw.xy = texcoord * uScaleUv + uBiasUv;
		uvw.z  = uLayer / float(textureSize(txTexture, int(uMip)).z);
		ret = textureLod(txTexture, uvw, uMip);
		
	#elif defined(TEXTURE_CUBE_MAP)
		vec2 uv = texcoord * uScaleUv + uBiasUv;
		vec3 uvw = Envmap_GetCubeFaceUvw(uv, int(uLayer));
		ret = textureLod(txTexture, uvw, uMip);
		
	#elif defined(TEXTURE_CUBE_MAP_ARRAY)
		vec2 uv = texcoord * uScaleUv + uBiasUv;		
		vec3 uvw = Envmap_GetCubeFaceUvw(uv, int(uLayer) % 6);
		ret = textureLod(txTexture, vec4(uvw, float(int(uLayer) / 6)), uMip);
		
	#else
		ret = vec4(1.0, 0.0, 0.0, 1.0);
	#endif

	if (bool(uIsDepth)) 
	{
		ret.rgb = textureLod(txRadar, vec2(fract(ret.r * 1024.0), 0.5), 0.0).rgb;
	}
	if (any(isnan(ret))) 
	{
		vec2 nanUv = vec2(gl_FragCoord.xy) / 16.0;
		nanUv.x = mix(0.5, 1.0, fract(nanUv.x));
		nanUv.y = 1.0 - nanUv.y;
		ret = vec4(textureLod(txRadar, nanUv, 0.0).a) + vec4(0.5, 0.0, 0.0, 1.0);
	}
	if (any(isinf(ret))) 
	{
		vec2 infUv = vec2(gl_FragCoord.xy) / 16.0;
		infUv.x = mix(0.0, 0.5, fract(infUv.x));
		infUv.y = 1.0 - infUv.y;
		ret = vec4(textureLod(txRadar, infUv, 0.0).a) + vec4(0.5, 0.25, 0.0, 1.0);
	}
	
	fResult = vec4(0.0, 0.0, 0.0, 1.0);
	bvec4 mask = bvec4(uRgbaMask);
	if (mask.r && !any(mask.gba)) 
	{
		fResult.rgb = ret.rrr;
	}
	else if (mask.g && !any(mask.rba)) 
	{
		fResult.rgb = ret.ggg;
	}
	else if (mask.b && !any(mask.rga)) 
	{
		fResult.rgb = ret.bbb;
	}
	else if (mask.a && !any(mask.rgb)) 
	{
		fResult.rgb = ret.aaa;
	} 
	else 
	{
	 // multiple color channels, grid for alpha
		if (mask.r) 
		{
			fResult.r = ret.r;
		}
		if (mask.g) 
		{
			fResult.g = ret.g;
		}
		if (mask.b) 
		{
			fResult.b = ret.b;
		}
		if (mask.a) 
		{
			vec2 gridUv = fract(gl_FragCoord.xy / 24.0);
			bool gridAlpha = (gridUv.x < 0.5);
			gridAlpha = (gridUv.y < 0.5) ? gridAlpha : !gridAlpha;
			fResult.rgb = mix(vec3(gridAlpha ? 0.2 : 0.3), fResult.rgb, ret.a);
		}
	}
}
