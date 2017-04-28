#include "shaders/def.glsl"
#include "shaders/Envmap.glsl"

noperspective in vec2 vUv;
noperspective in vec3 vFrustumRay;
noperspective in vec3 vFrustumRayW;

#if   defined(ENVMAP_CUBE)
	uniform samplerCube txEnvmap;
#elif defined(ENVMAP_SPHERE)
	uniform sampler2D txEnvmap;
#endif

layout(location=0) out vec3 fResult;

void main() 
{
#if   defined(ENVMAP_CUBE)
	vec3 ret = textureLod(txEnvmap, normalize(vFrustumRayW), 0.0).rgb;
#elif defined(ENVMAP_SPHERE)
	vec3 ret = textureLod(txEnvmap, Envmap_GetSphereUv(normalize(vFrustumRayW)), 0.0).rgb;
#endif
	fResult = ret;
}
