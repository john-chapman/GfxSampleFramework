#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#include "shaders/BasicRenderer/Lighting.glsl"

#define DebugViewMode_None              0
#define DebugViewMode_EnvironmentProbes 1
uniform int uMode;

layout(std430) restrict readonly buffer bfEnvProbes
{
	Lighting_EnvProbe uEnvProbes[];
};
uniform int uEnvProbeCount;
uniform samplerCubeArray txEnvProbes;

_VARYING(noperspective, vec2, vUv);
_VARYING(noperspective, vec3, vFrustumRay);
_VARYING(noperspective, vec3, vFrustumRayW);

_FRAGMENT_OUT(0, vec4, fResult);

void main()
{
	const vec3 P = Camera_GetPosition();
	const vec3 V = normalize(vFrustumRayW);

	switch (uMode)
	{
		default:
		case DebugViewMode_None:
		{
			fResult = vec4(vUv, 0.0, 1.0);
			break;
		}

		case DebugViewMode_EnvironmentProbes:
		{
			for (int i = 0; i < uEnvProbeCount; ++i)
			{
				vec3 R = V;
				if (uEnvProbes[i].originRadius.w > 0.0)
				{
				}
				else
				{
					// Box
					// https://github.com/EpicGames/UnrealEngine/blob/release/Engine/Shaders/Private/ReflectionEnvironmentShared.ush
					const vec3  boxOrigin = uEnvProbes[i].originRadius.xyz;
					const vec3  boxDistanceP = abs(P - boxOrigin) / uEnvProbes[i].boxHalfExtents.xyz;
					const float boxAttenuation = 1.0 - smoothstep(0.75, 1.0, max3(boxDistanceP));
					if (boxAttenuation > 0.0)
					{
						const vec3  boxMin    = boxOrigin - uEnvProbes[i].boxHalfExtents.xyz;
						const vec3  boxMax    = boxOrigin + uEnvProbes[i].boxHalfExtents.xyz;
					
						const vec3  boxMinP   = (boxMin - P) / R;
						const vec3  boxMaxP   = (boxMax - P) / R;
						const vec3  maxP      = max(boxMinP, boxMaxP);
						const float boxD      = min3(maxP);
					
						const vec3  boxIntersection = P + R * boxD;
						R = boxIntersection - boxOrigin;

						R.y = -R.y;
						fResult += textureLod(txEnvProbes, vec4(R, uEnvProbes[i].probeIndex), 0.0) * boxAttenuation;
					}
				}
			}
			break;
		}
	};
}
