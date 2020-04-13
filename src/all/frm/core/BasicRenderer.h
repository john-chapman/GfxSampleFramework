#pragma once

#include <frm/core/frm.h>
#include <frm/core/geom.h>
#include <frm/core/math.h>
#include <frm/core/types.h>
#include <frm/core/Camera.h>
#include <frm/core/RenderNodes.h>
#include <frm/core/RenderTarget.h>

#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>

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
// - Motion blur https://casual-effects.com/research/McGuire2012Blur/McGuire12Blur.pdf
//   - Polar representation for V? Allows direct loading of the vector magnitude.
//   - Tile min/max, neighborhood velocities at lower precision?
//   - Tile classification as per Jimenez.
// - TAA + interlacing.
// - Memory consumption/perf issues: some redundant render targets are allocated
//   to simplify the pipeline logic, there are also redundant calls to 
//   GlContext::blitFramebuffer() for the same reason (which have a not 
//   insignificant cost).
////////////////////////////////////////////////////////////////////////////////
struct BasicRenderer
{
	// Flags control some pipeline behaviour.
	enum Flag_
	{
		Flag_PostProcess,       // Enable default post processor (motion blur, tonemap). If disabled, txFinal must be written manually.
		Flag_TAA,               // Enable temporal antialiasing.
		Flag_FXAA,              // Enable FXAA.
		Flag_Interlaced,        // Enable interlaced rendering.
		Flag_WriteToBackBuffer, // Copy txFinal to the back buffer. Disable for custom upsampling/antialiasing.
		Flag_WireFrame,         // Wireframe overlay.

		Flags_Default = 0
			| (1 << Flag_PostProcess)
			| (1 << Flag_TAA)
			| (1 << Flag_FXAA)
			| (1 << Flag_WriteToBackBuffer)
	};
	typedef uint32 Flag;

	static BasicRenderer* Create(int _resolutionX, int _resolutionY, uint32 _flags = Flags_Default);
	static void Destroy(BasicRenderer*& _inst_);

	void draw(float _dt);
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

	AlignedBox      sceneBounds;                              // Bounding box for all renderables in the scene.
	AlignedBox      shadowSceneBounds;                        // Bounding box for all shadow-casting renderables.

	Framebuffer*    fbGBuffer                  = nullptr;     // txGBuffer0 + txGBufferDepthStencil.
	Framebuffer*    fbScene                    = nullptr;     // txScene + txGBufferDepth.
	Framebuffer*    fbPostProcessResult        = nullptr;     // txPostProcessResult + txGBufferDepthStencil.
	Framebuffer*    fbFXAAResult               = nullptr;     // txFXAAResult.
	Framebuffer*    fbFinal                    = nullptr;     // txFinal.

	TextureSampler* ssMaterial                 = nullptr;     // Sampler for material textures.
	Buffer*         bfMaterials                = nullptr;     // Material instance data.
	Buffer*         bfLights                   = nullptr;     // Basic light instance data.
	Buffer*         bfShadowLights             = nullptr;     // Shadow casting light instance data.
	Buffer*         bfImageLights              = nullptr;     // Image light instance data.
	Buffer*         bfPostProcessData	       = nullptr;     // Data for the post process shader.
	ShadowAtlas*    shadowAtlas                = nullptr;     // Shadow map allocations.
	
	Shader*         shStaticVelocity           = nullptr;     // Velocity fixup for static objects (i.e. camera-only velocity).
	Shader*         shVelocityMinMax           = nullptr;     // Generate tile min/max.
	Shader*         shVelocityNeighborMax      = nullptr;     // Generate tile neighbor max.
	Shader*         shImageLightBg             = nullptr;     // Environment map background shader.
	Shader*         shPostProcess              = nullptr;     // Motion blur, exposure, color grading & tonemapping.
	Shader*         shFXAA                     = nullptr;     // FXAA shader.
	Shader*         shTAAResolve               = nullptr;     // Resolve TAA.
	Shader*         shDepthClear               = nullptr;     // Used to clear subregions of the depth buffer.

	float           motionBlurTargetFps        = 60.0f;
	int             motionBlurTileWidth        = 20;
	float           taaSharpen                 = 0.4f;
	ivec2           resolution                 = ivec2(-1);
	uint32          flags                      = Flags_Default;
	bool            pauseUpdate                = false;
	bool            cullBySubmesh              = true;


private:
	BasicRenderer(int _resolutionX, int _resolutionY, uint32 _flags);
	~BasicRenderer();

	bool editFlag(const char* _name, Flag _flag);

	Camera sceneCamera;
	eastl::vector<Camera> shadowCameras;
	
	struct alignas(16) MaterialInstance 
	{
		vec4    baseColorAlpha = vec4(1.0f);
		vec4    emissiveColor  = vec4(0.0f);
		float   metallic       = 1.0f;
		float   roughness      = 1.0f;
		float   reflectance    = 1.0f;
		float   height         = 1.0f;
	};
	eastl::vector<MaterialInstance> materialInstances;
	void updateMaterialInstances();

	enum Pass_
	{
		Pass_Shadow,
		Pass_GBuffer,
		Pass_Scene,
		Pass_Wireframe,

		Pass_Count
	};
	typedef uint64 Pass;

	enum GeometryType_
	{
		GeometryType_Mesh,
		GeometryType_SkinnedMesh,

		GeometryType_Count
	};
	typedef uint64 GeometryType;

	union ShaderMapKey
	{
		struct
		{
			uint64 pass          : 8;  // Flag per pass.
			uint64 geometryType  : 8;  // Geometry type (static mesh, skinned mesh, etc.).
			uint64 rendererFlags : 8;  // Renderer-controlled flags (fade in/out, LOD transitions, etc.).
			uint64 materialFlags : 40; // Material flags (alpha test, alpha dither, etc.).
		} fields;
		uint64 value;

		operator uint64() const { return value; } 
	};
	using ShaderMap = eastl::unordered_map<ShaderMapKey, Shader*>;
	ShaderMap shaderMap;

	Shader* findShader(ShaderMapKey _key);

	struct alignas(16) DrawInstance
	{
		mat4   world          = identity;
		mat4   prevWorld      = identity;
		vec4   colorAlpha     = vec4(1.0f);
		uint32 materialIndex  = ~0;
		uint32 submeshIndex   = 0;
		uint32 skinningOffset = ~0;
	};
	struct DrawCall
	{
		const Shader*               shaders[Pass_Count] = { nullptr };
		const BasicMaterial*        material            = nullptr;
		const Mesh*                 mesh                = nullptr;
		uint32                      submeshIndex        = 0;
		Buffer*                     bfInstances         = nullptr;
		eastl::vector<DrawInstance> instanceData;

		// \todo This data can be shared between scene/shadow passes - split it out (but need to cull against *all* cameras).
		Buffer*                     bfSkinning          = nullptr;
		eastl::vector<mat4>         skinningData;
	};
	using DrawCallMap = eastl::unordered_map<uint64, DrawCall>;
	DrawCallMap sceneDrawCalls;
	eastl::vector<DrawCallMap> shadowDrawCalls;
	eastl::vector<void*> shadowMapAllocations; // \todo encapsule draw call map, camera and shadow allocation
	
	void updateDrawCalls();

	void addDrawCall(const Component_BasicRenderable* _renderable, int _submeshIndex, DrawCallMap& map_);
	void clearDrawCalls(DrawCallMap& map_);
	void bindAndDraw(const DrawCall& _drawCall);

	struct alignas(16) LightInstance
	{
		vec4  position      = vec4(0.0f); // A = type.
		vec4  direction     = vec4(0.0f);
		vec4  color         = vec4(0.0f); // RGB = color * brightness, A = brightness
		float invRadius2    = 0.0f;       // (1/radius)^2
		float spotScale     = 0.0f;       // 1 / saturate(cos(coneInner - coneOuter))
		float spotBias      = 0.0f;       // -coneOuter * scale;
		float _pad;
	};
	eastl::vector<LightInstance> lightInstances;

	struct alignas(16) ShadowLightInstance: public LightInstance
	{
		mat4  worldToShadow = identity;
		vec2  uvBias        = vec2(0.0f);
		float uvScale       = 1.0f;
		float arrayIndex    = 0.0f;
	};
	eastl::vector<ShadowLightInstance> shadowLightInstances;

	struct alignas(16) ImageLightInstance
	{
		float    brightness   = 1.0f;
		bool     isBackground = false;
		Texture* texture      = nullptr;
	};
	eastl::vector<ImageLightInstance> imageLightInstances;
	void updateImageLightInstances();

	struct alignas(16) PostProcessData
	{
		float motionBlurScale = 0.0f;     // current fps / target fps
	};
	PostProcessData postProcessData;

	void updateBuffer(Buffer*& bf_, const char* _name, GLsizei _size, void* _data);

 	LuminanceMeter luminanceMeter;

}; // class BasicRenderer

} // namespace frm
