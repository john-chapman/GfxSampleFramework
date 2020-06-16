#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#define GBuffer_IN
#include "shaders/BasicRenderer/GBuffer.glsl"
uniform sampler2D txPreviousGBuffer0;

#ifndef TAA
    #define TAA 0
#endif
#ifndef INTERLACED
    #define INTERLACED 0
#endif

uniform sampler2D txCurrent;                 // current scene rendering/FXAA result
uniform sampler2D txPrevious;                // previous scene rendering/FXAA result
uniform sampler2D txPreviousResolve;         // previous resolve result
uniform writeonly image2D txCurrentResolve;  // current resolve output
uniform writeonly image2D txFinal;           // blend history/current resolve

uniform int uFrameIndex; // 0 = even frame, horizontal; 1 = odd frame, vertical
uniform vec2 uResolveKernel;

void GetNeighborhoodBounds(in sampler2D _tx, in ivec2 _iuv, out vec3 min_, out vec3 max_, out vec3 center_)
{
    const ivec2 offsets[8] =
    {
        ivec2(-1, -1),
        ivec2( 0, -1),
        ivec2( 1, -1),
        ivec2(-1,  0),
        ivec2( 1,  0),
        ivec2(-1,  1),
        ivec2( 0,  1),
        ivec2( 1,  1)            
    };

    center_ = texelFetch(_tx, _iuv, 0).rgb;
    min_ = center_;
    max_ = center_;

    #if 1
     // basic color space bounds
        for (int i = 0; i < offsets.length(); ++i)
        {
            vec3 s = texelFetchOffset(_tx, _iuv, 0, offsets[i]).rgb;
            min_ = min(min_, s);
            max_ = max(max_, s);
        }
    #else
     // variance clipping (see https://developer.download.nvidia.com/gameworks/events/GDC2016/msalvi_temporal_supersampling.pdf)
        vec3 m1 = center_;
        vec3 m2 = m1 * m1;
        for (int i = 0; i < offsets.length(); ++i)
        {
            vec3 s = texelFetchOffset(_tx, _iuv, 0, offsets[i]).rgb;
            m1 += s;
            m2 += s * s;
            min_ = min(min_, s);
            max_ = max(max_, s);
        }
        vec3 mu = m1 / float(offsets.length());
		vec3 sigma = sqrt(m2 / float(offsets.length()) - mu * mu);
		const float kGamma = 2.0;
		min_ = max(min_, mu - kGamma * sigma);
		max_ = min(max_, mu + kGamma * sigma);
    #endif
}

void main()
{
	const ivec2 txSize = ivec2(imageSize(txFinal).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
    {
		return;
	}
	const vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);
    const ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    
    vec3 retFinal = vec3(0.0);

    #if INTERLACED
    {
        const ivec2 iuv2 = ivec2(iuv.x / 2, iuv.y);
        
        retFinal = texelFetch(txCurrent, iuv2, 0).rgb;

        if ((iuv.x & 1) != uFrameIndex)
        {
            const vec2 velocity = GBuffer_ReadVelocity(iuv2);
	        const vec2 prevUv = uv - velocity;
            const ivec2 iuvPrev = ivec2(prevUv.x * txSize.x / 2.0, prevUv.y * txSize.y);

            float weight = any(greaterThanEqual(abs(prevUv * 2.0 - 1.0), vec2(1.0))) ? 0.0 : 1.0;
            retFinal = mix(retFinal, texelFetch(txPrevious, iuvPrev, 0).rgb, weight);
        }
    
        #if !TAA
            imageStore(txFinal, iuv, vec4(retFinal, 1.0));
        #endif
    }
    #endif

    #if TAA
    {
        vec3 retResolve = vec3(0.0);
    
        // 4-tap sharpening filter
        // \todo bilinear lookup?
        const int kKernelSize = 4;
        const float kKernelOffsets[kKernelSize] = { -2.0, -1.0, 0.0, 1.0 };
        const float kKernelWeights[kKernelSize] = { uResolveKernel.x, uResolveKernel.y, uResolveKernel.y, uResolveKernel.x };

        vec2 texelSize = vec2(1.0) / txSize;
        for (int i = 0; i < kKernelSize; ++i)
        {
            vec2 sampleUv = uv;
            sampleUv[uFrameIndex] += kKernelOffsets[i] * texelSize[uFrameIndex] * 1.0;
            retResolve += textureLod(txCurrent, sampleUv, 0.0).rgb * kKernelWeights[i];
        }

        #if INTERLACED
            const ivec2 iuv2 = ivec2(gl_GlobalInvocationID.x / 2, gl_GlobalInvocationID.y);
        #else
            const ivec2 iuv2 = iuv;
        #endif
    
        vec3 retFinal = vec3(0.0);
        vec3 localMin, localMax, center;
        GetNeighborhoodBounds(txCurrent, iuv2, localMin, localMax, center);

        const vec2 velocity = GBuffer_ReadVelocity(iuv2);
        const vec2 prevUv = uv - velocity;
        vec3 prevColor = textureLod(txPreviousResolve, prevUv, 0.0).rgb;

        prevColor = clamp(prevColor, localMin, localMax); // \todo this introduces jitter?
        retFinal = mix(retResolve, prevColor, 0.5);

        imageStore(txCurrentResolve, iuv, vec4(retResolve, 1.0));
        imageStore(txFinal, iuv, vec4(retFinal, 1.0));
    }
    #endif
}
