#include "shaders/def.glsl"
#include "shaders/Envmap.glsl"
#include "shaders/Camera.glsl"

noperspective in vec2 vUv;
noperspective in vec3 vFrustumRay;
noperspective in vec3 vFrustumRayW;

#if   defined(ENVMAP_CUBE)
	uniform samplerCube txEnvmap;
#elif defined(ENVMAP_SPHERE)
	uniform sampler2D txEnvmap;
#endif

layout(location=0) out vec4 fResult;

void main()
{
#if   defined(ENVMAP_CUBE)
	vec3 ret = textureLod(txEnvmap, normalize(vFrustumRayW), 0.0).rgb;
#elif defined(ENVMAP_SPHERE)
	vec3 ret = textureLod(txEnvmap, Envmap_GetSphereUv(normalize(vFrustumRayW)), 0.0).rgb;
#endif

#if defined(GAMMA)
	ret = Gamma_Apply(ret);
#endif

	// BasicRenderer writes linear depth to the scene buffer's alpha channel, hence write a sensible value when this shader is used to clear the screen
	float alpha = abs(uCamera.m_far);

	fResult = vec4(ret, alpha);
}
