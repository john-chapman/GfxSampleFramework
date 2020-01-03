#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#define GBuffer_OUT
#include "shaders/BasicRenderer/GBuffer.glsl"

uniform sampler2D txGBufferDepth;

_VARYING(noperspective, vec2, vUv);
_VARYING(noperspective, vec3, vFrustumRay);
_VARYING(noperspective, vec3, vFrustumRayW);

void main()
{
    const float depth = texelFetch(txGBufferDepth, ivec2(gl_FragCoord.xy), 0).x;
    const vec3 P = Camera_GetPosition() - vFrustumRayW * Camera_GetDepthV(depth);
    vec4 prev = uCamera.m_prevViewProj * vec4(P, 1.0);
    prev.xy /= prev.w;
    vec2 velocity = vUv - (prev.xy * 0.5 + 0.5);

    // Correct for any jitter in the projection matrix.
    // Unlike in the material shader, we only need to account for the *previous* projection because vUv isn't jittered.
    vec2 jitterVelocity = uCamera.m_prevProj[2].xy * 0.5;
    velocity -= jitterVelocity;

    GBuffer_WriteVelocity(velocity);
}
