#include "shaders/def.glsl"

#define FXAA_GLSL_130        1
#define FXAA_PC              1
#define FXAA_DISCARD         0  // \todo test perf
#define FXAA_QUALITY__PRESET 29
#include "shaders/BasicRenderer/FXAA_311.glsl"

uniform float uTexelScaleX; // Scale texel size to compensate for interlaced rendering.
uniform sampler2D txIn;
uniform writeonly image2D txOut;

void main()
{
    const ivec2 txSize = ivec2(imageSize(txOut).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
    {
		return;
	}
	const vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);
    const ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    const vec2 texelSize = 1.0 / vec2(txSize) * vec2(uTexelScaleX, 1.0); 

    vec4 ret = FxaaPixelShader(
        uv,            // pos
        vec4(0.0),     // fxaaConsolePosPos
        txIn,          // tex
        txIn,          // fxaaConsole360TexExpBiasNegOne
        txIn,          // fxaaConsole360TexExpBiasNegTwo
        texelSize,     // fxaaQualityRcpFrame
        vec4(0.0),     // fxaaConsoleRcpFrameOpt
        vec4(0.0),     // fxaaConsoleRcpFrameOpt2
        vec4(0.0),     // fxaaConsole360RcpFrameOpt2
        0.75,          // fxaaQualitySubpix
        0.125,         // fxaaQualityEdgeThreshold
        0.0625,        // fxaaQualityEdgeThresholdMin
        8.0,           // fxaaConsoleEdgeSharpness
        0.0,           // fxaaConsoleEdgeThreshold
        0.0,           // fxaaConsoleEdgeThresholdMin
        vec4(0.0)      // fxaaConsole360ConstDir
        );

    //ret = textureLod(txIn, uv, 0.0);
    
    imageStore(txOut, iuv, ret);
}
