#include "shaders/def.glsl"

uniform sampler2D txVelocityTileMinMax;
uniform writeonly image2D txVelocityTileNeighborMax;

#define TILE_WIDTH (gl_WorkGroupSize.x)
#define TILE_COORD (gl_LocalInvocationID.x)
#define TILE_XY    (gl_WorkGroupID.xy * TILE_WIDTH)

shared vec4 s_cache[TILE_WIDTH];

void main()
{
    const ivec2 txSize = ivec2(imageSize(txVelocityTileNeighborMax).xy);
    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
    {
	    return;
    }
    const vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);
    const ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    const vec2 texelSize = 1.0 / vec2(txSize);

    const vec2 kOffsets[8] = 
    {
        vec2(-1.0, -1.0) * texelSize,
        vec2( 0.0, -1.0) * texelSize,
        vec2( 1.0, -1.0) * texelSize,
        vec2(-1.0,  0.0) * texelSize,
        vec2( 1.0,  0.0) * texelSize,
        vec2(-1.0,  1.0) * texelSize,
        vec2( 0.0,  1.0) * texelSize,
        vec2( 1.0,  1.0) * texelSize
    };
    vec2  neighborMax = textureLod(txVelocityTileMinMax, uv, 0.0).xy * 2.0 - 1.0;
    float maxLength   = length2(neighborMax);
    for (int i = 0; i < kOffsets.length(); ++i)
    {
        const vec2 value = textureLod(txVelocityTileMinMax, uv + kOffsets[i], 0.0).xy * 2.0 - 1.0;
        const float valueLength = length2(value);
        if (valueLength > maxLength)
        {
        neighborMax = value;
        maxLength = valueLength;
        }
    }
    neighborMax = neighborMax * 0.5 + 0.5;
    imageStore(txVelocityTileNeighborMax, iuv, neighborMax.xyxy);
}
