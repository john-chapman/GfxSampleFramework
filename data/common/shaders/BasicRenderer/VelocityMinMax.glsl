#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#define GBuffer_IN
#include "shaders/BasicRenderer/GBuffer.glsl"

uniform writeonly image2D txVelocityTileMinMax;

#define TILE_WIDTH (gl_WorkGroupSize.x)
#define TILE_COORD (gl_LocalInvocationID.x)
#define TILE_XY    (gl_WorkGroupID.xy * TILE_WIDTH)

shared vec4 s_cache[TILE_WIDTH];

void main()
{
    const ivec2 txSize = ivec2(imageSize(txVelocityTileMinMax).xy);
    if (any(greaterThanEqual(gl_WorkGroupID.xy, txSize)))
    {
	    return;
    }

    // For each row in the tile, compute the min/max.
    // \todo Separate cache, loop over rows + parallel reduction per row?
    {
        s_cache[TILE_COORD].xy = GBuffer_ReadVelocity(ivec2(TILE_XY.x, TILE_XY.y + gl_LocalInvocationID.x));
        s_cache[TILE_COORD].zw = s_cache[TILE_COORD].xy;
        float maxLength = length2(s_cache[TILE_COORD].xy);
        float minLength = length2(s_cache[TILE_COORD].zw);
        for (uint i = 1; i < TILE_WIDTH; ++i)
        {
            vec2 value = GBuffer_ReadVelocity(ivec2(TILE_XY.x + i, TILE_XY.y + gl_LocalInvocationID.y));
            float valueLength = length2(value);
            if (valueLength > maxLength)
            {
                s_cache[TILE_COORD].xy = value;
                maxLength = valueLength;
            }
            if (valueLength < minLength)
            {
                s_cache[TILE_COORD].zw = value;
                minLength = valueLength;
            }
        }
    }
    memoryBarrierShared();
    barrier();

    // Min/max for the cache is now min/max for the tile.
    // \todo parallel reduction
    if (gl_LocalInvocationID.x == 0)
    {
        float maxLength = length2(s_cache[0].xy);
        float minLength = length2(s_cache[0].zw);
        for (uint i = 1; i < TILE_WIDTH; ++i)
        {
            vec4 value = s_cache[i];
            float valueMaxLength = length2(value.xy);
            float valueMinLength = length2(value.zw);
            if (valueMaxLength > maxLength)
            {
                s_cache[0].xy = value.xy;
                maxLength = valueMaxLength;
            }
            if (valueMinLength < minLength)
            {
                s_cache[0].zw = value.zw;
                minLength = valueMinLength;
            }
        }
        vec4 tileMinMax = s_cache[0] * 0.5 + 0.5;
        imageStore(txVelocityTileMinMax, ivec2(gl_WorkGroupID.xy), tileMinMax);
    }
}
