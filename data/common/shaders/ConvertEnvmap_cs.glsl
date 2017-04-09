#include "shaders/def.glsl"
#include "shaders/Envmap.glsl"

// adapted from http://paulbourke.net/miscellaneous/cubemaps/

#ifdef CUBE_TO_SPHERE
	#if defined(SPHERE_TO_CUBE)
		#error Multiple conversions defined
	#endif
	uniform samplerCube txCube;
	uniform image2D writeonly txSphere;
#endif

#ifdef SPHERE_TO_CUBE
	#if defined(CUBE_TO_SPHERE)
		#error Multiple conversions defined
	#endif
	uniform sampler2D txSphere;
	uniform imageCube writeonly txCube;
#endif

void main()
{
	vec2 uv = vec2(gl_WorkGroupID.xy) / vec2(gl_NumWorkGroups.xy);
	
#ifdef CUBE_TO_SPHERE
	ivec2 iuv = ivec2(uv * vec2(imageSize(txSphere)));
	uv = uv * 2.0 - 1.0;
	float theta = uv.x * kPi;
	float phi = uv.y * kPi * 0.5; // or asin(uv.y)
	vec3 uvw = vec3(
		cos(phi) * cos(theta),
		sin(phi),
		cos(phi) * sin(theta)
		);
	vec4 ret = textureLod(txCube, uvw, 0.0);
	imageStore(txSphere, iuv, ret);
#endif

#ifdef SPHERE_TO_CUBE
	ivec2 iuv = ivec2(uv * vec2(imageSize(txCube)));
	vec3 uvw = Envmap_GetCubeFaceUvw(uv, int(gl_WorkGroupID.z));
	uv = Envmap_GetSphereUv(normalize(uvw));
	vec4 ret = textureLod(txSphere, uv, 0.0);
	imageStore(txCube, ivec3(iuv, int(gl_WorkGroupID.z)), ret);
#endif
}
