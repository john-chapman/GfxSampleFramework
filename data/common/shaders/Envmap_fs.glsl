#include "shaders/def.glsl"
#include "shaders/Envmap.glsl"

noperspective in vec3 vViewRayW;

#if   defined(ENVMAP_CUBE)
	uniform samplerCube txEnvmap;
#elif defined(ENVMAP_SPHERE)
	uniform sampler2D txEnvmap;
#endif

layout(location=0) out vec3 fResult;

void main() 
{
#if   defined(ENVMAP_CUBE)
	vec3 ret = textureLod(txEnvmap, normalize(vViewRayW), 0.0).rgb;
#elif defined(ENVMAP_SPHERE)
	vec3 ret = textureLod(txEnvmap, Envmap_GetSphereUv(normalize(vViewRayW)), 0.0).rgb;
#endif
	fResult = ret;
}
