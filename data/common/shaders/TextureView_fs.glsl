#include "shaders/def.glsl"

noperspective in vec2 vUv;
noperspective in vec4 vColor;

uniform vec2  uScaleUv;
uniform vec2  uBiasUv;
uniform float uLayer;
uniform float uMip;
uniform uvec4 uRgbaMask;
uniform int   uIsDepth;

#if   defined(TEXTURE_1D)
	#define TextureType sampler1D
#elif defined(TEXTURE_1D_ARRAY)
	#define TextureType sampler1DArray
#elif defined(TEXTURE_2D)
	#define TextureType sampler2D
#elif defined(TEXTURE_2D_ARRAY)
	#define TextureType sampler2DArray
#elif defined(TEXTURE_3D)
	#define TextureType sampler3D
#else
	//#error TextureVis_fs: No texture type defined.
#endif

#ifdef TextureType
	uniform TextureType txTexture;
#endif

layout(location=0) out vec4 fResult;

void main() 
{
	vec4 ret;
	vec2 texcoord = vUv;//vec2(vUv.x, 1.0 - vUv.y);
	#if   defined(TEXTURE_1D)
		ret = textureLod(txTexture, vUv.x * uScaleUv.x + uBiasUv.x, uMip);
		
	#elif defined(TEXTURE_1D_ARRAY)
		vec2 uv;
		uv.x = texcoord.x * uScaleUv.x + uBiasUv.x;
		uv.y = uLayer;
		ret = textureLod(txTexture, uv, uMip);
		
	#elif defined(TEXTURE_2D)
		ret = textureLod(txTexture, texcoord * uScaleUv + uBiasUv, uMip);

	#elif defined(TEXTURE_2D_ARRAY)
		vec3 uvw;
		uvw.xy = texcoord * uScaleUv + uBiasUv;
		uvw.z  = uLayer;
		ret = textureLod(txTexture, uvw, uMip);
		
	#elif defined(TEXTURE_3D)
		vec3 uvw;
		uvw.xy = texcoord * uScaleUv + uBiasUv;
		uvw.z  = uLayer / float(textureSize(txTexture, int(uMip)).z);
		ret = textureLod(txTexture, uvw, uMip);

	#else
		ret = vec4(1.0, 0.0, 0.0, 1.0);
	#endif

	if (bool(uIsDepth)) {
		ret.rgb = vec3(fract(ret.r * 1024.0)); // \todo better depth/stencil vis
	}
	
	fResult = ret;
	fResult.a = 1.0;

	fResult = vec4(0.0, 0.0, 0.0, 1.0);

	
	bvec4 mask = bvec4(uRgbaMask);
	if (mask.a && !any(mask.rgb)) {
	 // draw alpha channel as monochrome
		fResult.rgb = vec3(ret.a);
	} else {
	 // draw rgb, grid for alpha
		if (mask.r) {
			fResult.r = ret.r;
		}
		if (mask.g) {
			fResult.g = ret.g;
		}
		if (mask.b) {
			fResult.b = ret.b;
		}
		if (mask.a) {
			vec2 gridUv = fract(gl_FragCoord.xy / 24.0);
			bool gridAlpha = (gridUv.x < 0.5);
			gridAlpha = (gridUv.y < 0.5) ? gridAlpha : !gridAlpha;
			fResult.rgb = mix(vec3(gridAlpha ? 0.2 : 0.3), fResult.rgb, ret.a);
		}
	}
}
