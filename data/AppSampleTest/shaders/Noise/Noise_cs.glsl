#include "shaders/def.glsl"
#include "shaders/Noise.glsl"

// defines noise function to use: Noise_fBm, Noise_Turbulence, Noise_Ridge
#ifndef NOISE
	#define NOISE Noise_fBm
#endif

uniform vec2  uScale;
uniform vec2  uBias;
uniform float uFrequency;
uniform float uLacunarity;
uniform float uGain;
uniform int   uLayers;

uniform writeonly image2D txOut;

void main()
{
	vec2 txSize = vec2(imageSize(txOut).xy);
	if (any(greaterThanEqual(ivec2(gl_GlobalInvocationID.xy), ivec2(txSize)))) {
		return;
	}
 	ivec2 iuv  = ivec2(gl_GlobalInvocationID.xy);
	vec2  uv   = uBias + (vec2(iuv) / txSize + 0.5 / txSize) * uScale;

	float ret = NOISE(uv, uFrequency, uLacunarity, uGain, uLayers);
	
	imageStore(txOut, iuv, vec4(ret));
}
