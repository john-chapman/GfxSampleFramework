/*
    Nomenclature
    ============    
    P = point being shaded.
    N = surface normal at P.
    L = direction to the light.
    V = direction from P to the view position.
    H = half vector between V and L.

    References
    ==========
    1. https://google.github.io/filament/Filament.html
*/
#ifndef Lighting_glsl
#define Lighting_glsl

#define Lighting_PREDICATE_NoL  0 // early-out of lighting calculations if NoL == 0
#define Lighting_EPSILON (1e-4)

struct Lighting_In
{
 // Geometric terms derived from N, L, V and H.
	float NoL;
	float NoV;
	float NoH;
	float VoH;
    float LoH;

 // Material properties.
    vec3  diffuse;  // Diffuse reflectance.
	vec3  f0;       // Reflectance at normal incidence.
	float f90;      // Reflectance at oblique angles.
	float alpha;    // Remapped material roughness.
};

#define LightType_Direct 0
#define LightType_Point  1
#define LightType_Spot   2
#define LightType_Count  3

struct Lighting_Light
{
 // \todo pack
	vec4 position;    // A = light type 
	vec4 direction;
	vec4 color;       // RGB = color * brightness, A = brightness
	vec4 attenuation; // X,Y = linear attenuation start,stop, Z,W = radial attenuation start,stop
};

void Lighting_Init(
    inout Lighting_In _in_,
    in    vec3        _N,                    // Surface normal at P.
    in    vec3        _V,                    // Direction from P to the view position.
    in    float       _perceptualRoughness,  // Material roughness.
    in    vec3        _baseColor,            // Material base color (linear).
    in    float       _metal,                // Material metal mask.
    in    float       _reflectance           // Material reflectance (dielectrics only).
    )
{
    _in_.NoV     = clamp(abs(dot(_N, _V)), Lighting_EPSILON, 1.0);
    _in_.diffuse = _baseColor * (1.0 - _metal);
	_in_.f0      = 0.16 * _reflectance * _reflectance * (1.0 - _metal) + _baseColor * _metal;
    _in_.f90     = 1.0; // \todo
    _in_.alpha   = clamp(_perceptualRoughness * _perceptualRoughness, Lighting_EPSILON, 1.0);
}

void Lighting_InitLight(
    inout Lighting_In _in_,    
    in    vec3        _N,  // Surface normal at P.
    in    vec3        _V,  // Direction from P to the view position.
    in    vec3        _L   // Direction to the light.
    )
{
	vec3 H  = normalize(_V + _L);
	_in_.NoL = clamp(dot(_N, _L), Lighting_EPSILON, 1.0);
	_in_.NoH = clamp(dot(_N, H),  0.0,              1.0);
	_in_.VoH = clamp(dot(_V, H),  0.0,              1.0);
	_in_.LoH = clamp(dot(_L, H),  0.0,              1.0);
}

// Normal distribution.
float Lighting_SpecularD_GGX(in Lighting_In _in)
{
    float a = _in.NoH * _in.alpha;
    float k = _in.alpha / (1.0 - _in.NoH * _in.NoH + a * a);
    return k * k * (1.0 / kPi);
}
#define Lighting_SpecularD(_in) Lighting_SpecularD_GGX(_in)

// Geometric shadowing/masking.
float Lighting_SpecularG_SmithGGXCorrelated(in Lighting_In _in)
{
    #if 0 // correct
    {
        float a2 = _in.alpha * _in.alpha;
        float v = _in.NoL * sqrt(_in.NoV * _in.NoV * (1.0 - a2) + a2); // \todo can be precomputed for all lights
        float l = _in.NoV * sqrt(_in.NoL * _in.NoL * (1.0 - a2) + a2);
        return 0.5 / (v + l);
    }
    #else // approximate
    {
        float a = _in.alpha;
        float v = _in.NoL * (_in.NoV * (1.0 - a) + a); // \todo can be precomputed for all lights
        float l = _in.NoV * (_in.NoL * (1.0 - a) + a);
        return 0.5 / (v + l);
    }
    #endif
}
#define Lighting_SpecularG(_in) Lighting_SpecularG_SmithGGXCorrelated(_in)

// Fresnel.
vec3 Lighting_SpecularF_Schlick(in Lighting_In _in)
{
    #if 1 // correct
    {
        return _in.f0 + (vec3(_in.f90) - _in.f0) * pow(1.0 - _in.VoH, 5.0);
    }
    #else // approximate
    {        
		return _in.f0 + (vec3(_in.f90) - _in.f0) * pow(2.0, (_in.VoH * -5.55473 - 6.98316) * _in.VoH);
    }
    #endif
}
#define Lighting_SpecularF(_in) Lighting_SpecularF_Schlick(_in)

// Diffuse.
vec3 Lighting_Diffuse_Lambert(in Lighting_In _in)
{
    return _in.diffuse * (1.0 / kPi);
}
float Lighting_Diffuse_Schlick(in float _VoH, in float _f90) // helper for Lighting_Diffuse_Burley
{
    #if 1 // correct
    {
        return 1.0 + (_f90 - 1.0) * pow(1.0 - _VoH, 5.0);
    }
    #else // approximate
    {        
		return 1.0 + (_f90 - 1.0) * pow(2.0, (_VoH * -5.55473 - 6.98316) * _VoH));
    }
    #endif
}
vec3 Lighting_Diffuse_Burley(in Lighting_In _in)
{
    float f90 = 0.5 + 2.0 * _in.alpha * _in.LoH * _in.LoH;
    float l = Lighting_Diffuse_Schlick(_in.NoL, f90);
    float v = Lighting_Diffuse_Schlick(_in.NoV, f90);
    return _in.diffuse * (l * v * (1.0 / kPi));
}
#define Lighting_Diffuse(_in) Lighting_Diffuse_Lambert(_in)

float Lighting_DistanceAttenuation(in float _distance2, in float _falloff)
{
    float d = _distance2 * _falloff;
          d = saturate(1.0 - d * d);
    return (d * d) / max(_distance2, 1e-4);
}

float Lighting_AngularAttenuation(in float _distance2, in float _falloff)
{
    float d = _distance2 * _falloff;
          d = saturate(1.0 - d * d);
    return (d * d) / max(_distance2, 1e-4);
}

vec3 Lighting_Common(
    inout Lighting_In    _in_,
    in    Lighting_Light _light
    )
{
    if (bool(Lighting_PREDICATE_NoL) && _in_.NoL < 1e-7)
    {
        return vec3(0.0);
    }

    float D  = Lighting_SpecularD(_in_);
    float G  = Lighting_SpecularG(_in_);
    vec3  F  = Lighting_SpecularF(_in_);
    vec3  Fr = (D * G) * F;
    vec3  Fd = Lighting_Diffuse(_in_);

    return (Fr + Fd) * _light.color.rgb * _in_.NoL;
}

vec3 Lighting_Direct(
    inout Lighting_In    _in_,
    in    Lighting_Light _light,
    in    vec3           _N,
    in    vec3           _V
    )
{
    Lighting_InitLight(_in_, _N, _V, _light.direction.xyz);
    return Lighting_Common(_in_, _light);
}

vec3 Lighting_Point(
    inout Lighting_In    _in_,
    in    Lighting_Light _light,
    in    vec3           _P,
    in    vec3           _N,
    in    vec3           _V
    )
{
    vec3  L   = _light.position.xyz - _P;
	float d2  = length2(L);
	      L  /= sqrt(d2);
    Lighting_InitLight(_in_, _N, _V, L);
    _in_.NoL *= Lighting_DistanceAttenuation(d2, _light.attenuation.x);
    return Lighting_Common(_in_, _light);
}

vec3 Lighting_Spot(
    inout Lighting_In    _in_,
    in    Lighting_Light _light,
    in    vec3           _P,
    in    vec3           _N,
    in    vec3           _V
    )
{
    vec3  L   = _light.position.xyz - _P;
	float d2  = length2(L);
	      L  /= sqrt(d2);
    Lighting_InitLight(_in_, _N, _V, L);
    _in_.NoL *= Lighting_DistanceAttenuation(d2, _light.attenuation.x);
    _in_.NoL *= 1.0 - smoothstep(_light.attenuation.z, _light.attenuation.w, 1.0 - dot(_light.direction.xyz, L));
    return Lighting_Common(_in_, _light);
}


#endif // Lighting_glsl
