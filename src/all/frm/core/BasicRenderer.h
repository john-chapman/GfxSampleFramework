#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/types.h>
#include <frm/core/Camera.h>
#include <frm/core/RenderNodes.h>
#include <frm/core/RenderTarget.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// BasicRenderer
//
// Basic scene renderer with a prepass for depth, normal, velocity. 
// See Component_BasicRenderable, Component_BasicLight, Component_ImageLight.
//
// - Velocity rendering uses the camera's current and previous projection 
//   matrices to extract and compensate for XY jitter.
//
// \todo
// - Light attenuation broken?
// - Motion blur https://casual-effects.com/research/McGuire2012Blur/McGuire12Blur.pdf
//   - Polar representation for V? Allows direct loading of the vector magnitude.
//   - Tile min/max, neighborhood velocities at lower precision?
//   - Tile classification as per Jimenez.
// - Shadow system (gather shadow casting light components, allocate shadow map
//   resolution from an atlas).
////////////////////////////////////////////////////////////////////////////////
struct BasicRenderer
{
	// Flags control some pipeline behaviour.
	enum Flag_
	{
		Flag_PostProcess,       // Enable default post processor (motion blur, tonemap). If disabled, txFinal must be written manually.
		Flag_TAA,               // Enable temporal antialiasing.
		Flag_FXAA,              // Enable FXAA.
		Flag_Interleaved,       // Enable interleaved rendering.
		Flag_WriteToBackBuffer, // Copy txFinal to the back buffer. Disable for custom upsampling/antialiasing.

		Flags_Default = 0
			| (1 << Flag_PostProcess)
			| (1 << Flag_TAA)
			| (1 << Flag_FXAA)
			| (1 << Flag_WriteToBackBuffer)
	};
	typedef uint32 Flag;

	static BasicRenderer* Create(int _resolutionX, int _resolutionY, uint32 _flags = Flags_Default);
	static void Destroy(BasicRenderer*& _inst_);

	void draw(Camera* _camera, float _dt);
	bool edit();

	void setResolution(int _resolutionX, int _resolutionY);

	void setFlag(Flag _flag, bool _value) { flags = BitfieldSet(flags, (int)_flag, _value); }
	bool getFlag(Flag _flag) const        { return BitfieldGet(flags, (uint32)_flag); }
	
	enum Target_
	{
		Target_GBuffer0,                 // Normal, velocity.
		Target_GBufferDepthStencil,      // Depth, stencil.
		Target_VelocityTileMinMax,       // Min,max velocity per tile.
		Target_VelocityTileNeighborMax,  // Max velocity in 3x3 tile neighborhood.
		Target_Scene,                    // Lighting accumulation, etc.
		Target_PostProcessResult,        // Post processing result, alpha = luminance.
		Target_FXAAResult,               // FXAA result (can't write directly to Target_Final if TAA is enabled).
		Target_TAAResolve,               // Result of any AA resolve.
		Target_Final,                    // Backbuffer proxy.

		Target_Count
	};
	typedef int Target;
	RenderTarget renderTargets[Target_Count];

	void initRenderTargets();
	void shutdownRenderTargets();

	void initShaders();
	void shutdownShaders();

	Camera          camera;

	Framebuffer*    fbGBuffer                  = nullptr; // txGBuffer0 + txGBufferDepthStencil.
	Framebuffer*    fbScene                    = nullptr; // txScene + txGBufferDepth.
	Framebuffer*    fbPostProcessResult        = nullptr; // txPostProcessResult + txGBufferDepthStencil.
	Framebuffer*    fbFXAAResult               = nullptr; // txFXAAResult.
	Framebuffer*    fbFinal                    = nullptr; // txFinal + txGBufferDepthStencil.

	TextureSampler* ssMaterial                 = nullptr; // Sampler for material textures.
	Buffer*         bfMaterials                = nullptr; // Material instance data.
	Buffer*         bfLights                   = nullptr; // Basic light instance data.
	Buffer*         bfImageLights              = nullptr; // Image light instance data.
	Buffer*         bfPostProcessData	       = nullptr; // Data for the post process shader.
	
	Shader*         shGBuffer                  = nullptr; // Geometry shader for GBuffer pass.
	Shader*         shStaticVelocity           = nullptr; // Velocity fixup for static objects (i.e. camera-only velocity).
	Shader*         shVelocityMinMax           = nullptr; // Generate tile min/max.
	Shader*         shVelocityNeighborMax      = nullptr; // Generate tile neighbor max.
	Shader*         shImageLightBg             = nullptr; // Environment map background shader.
	Shader*         shScene                    = nullptr; // Geometry shader for scene pass (do lighting, etc.).
	Shader*         shPostProcess              = nullptr; // Motion blur, exposure, color grading & tonemapping.
	Shader*         shFXAA                     = nullptr; // FXAA shader.
	Shader*         shTAAResolve               = nullptr; // Resolve TAA.

	float           motionBlurTargetFps        = 60.0f;
	int             motionBlurTileWidth        = 20;
	ivec2           resolution                 = ivec2(-1);
	uint32          flags                      = Flags_Default;
	bool            pauseUpdate                = false;

private:
	BasicRenderer(int _resolutionX, int _resolutionY, uint32 _flags);
	~BasicRenderer();

	bool editFlag(const char* _name, Flag _flag);

	struct alignas(16) MaterialInstance 
	{
		vec4    baseColorAlpha = vec4(1.0f);
		vec4    emissiveColor  = vec4(0.0f);
		float   metallic       = 1.0f;
		float   roughness      = 1.0f;
		float   reflectance    = 1.0f;
		float   height         = 1.0f;
		uint32  flags          = 0u;
	};
	eastl::vector<MaterialInstance> materialInstances;
	void updateMaterialInstances();

	struct alignas(16) DrawInstance
	{
		Mesh*  mesh          = nullptr;
		mat4   world         = identity;
		mat4   prevWorld     = identity;
		vec4   colorAlpha    = vec4(1.0f);
		uint32 materialIndex = ~0;
		uint32 submeshIndex  = 0;
	};
	typedef eastl::vector<eastl::vector<DrawInstance> > DrawInstanceMap; // [material index][instance]
	DrawInstanceMap drawInstances;
	void updateDrawInstances(const Camera* _camera);

	struct alignas(16) LightInstance
	{
	 // \todo pack
		vec4 position      = vec4(0.0f); // A = type.
		vec4 direction     = vec4(0.0f);
		vec4 color         = vec4(0.0f); // RGB = color * brightness, A = brightness
		vec4 attenuation   = vec4(0.0f); // X,Y = linear attenuation start,stop, Z,W = radial attenuation start,stop
	};
	eastl::vector<LightInstance> lightInstances;
	void updateLightInstances(const Camera* _camera);

	struct alignas(16) ImageLightInstance
	{
		float    brightness   = 1.0f;
		bool     isBackground = false;
		Texture* texture      = nullptr;
	};
	eastl::vector<ImageLightInstance> imageLightInstances;
	void updateImageLightInstances(const Camera* _camera);

	struct alignas(16) PostProcessData
	{
		float motionBlurScale = 0.0f;     // current fps / target fps
	};
	PostProcessData postProcessData;

 	LuminanceMeter luminanceMeter;

}; // class BasicRenderer

} // namespace frm
