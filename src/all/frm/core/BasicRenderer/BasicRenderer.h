#pragma once

#include <frm/core/frm.h>
#include <frm/core/geom.h>
#include <frm/core/math.h>
#include <frm/core/types.h>
#include <frm/core/BitFlags.h>
#include <frm/core/Camera.h>
#include <frm/core/RenderTarget.h>

#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>

#include <functional>

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
// - Memory consumption/perf issues: some redundant render targets are allocated
//   to simplify the pipeline logic, there are also redundant calls to
//   GlContext::blitFramebuffer() for the same reason (which have a not
//   insignificant cost).
//
// \todo VR
// - TAA/interlacing (can't extract jitter from the proj matrix directly).
// - Quality options (for post processing, TAA, etc.).
// - Temporal scene RTs need to be stereoscopic.
// - Write directly to the HMD backbuffer.
// - Reprojection stereo for background (alternate the 'source' eye between frames?).
////////////////////////////////////////////////////////////////////////////////
class BasicRenderer
{
public:

	enum class Flag
	{
		PostProcess,       // Enable default post processor (motion blur, tonemap). If disabled, txFinal must be written manually.
		TAA,               // Enable temporal antialiasing.
		FXAA,              // Enable FXAA.
		Interlaced,        // Enable interlaced rendering.
		WriteToBackBuffer, // Copy txFinal to the back buffer. Disable for custom upsampling/antialiasing.
		WireFrame,         // Wireframe overlay.

		_Count,
		_Default = BIT_FLAGS_DEFAULT(PostProcess, TAA, FXAA, WriteToBackBuffer)
	};
	using Flags = BitFlags<Flag>;

	static BasicRenderer* Create(Flags _flags = Flags());
	static void Destroy(BasicRenderer*& _inst_);

	void nextFrame(float _dt, Camera* _drawCamera, Camera* _cullCamera);
	void draw(float _dt, Camera* _drawCamera, Camera* _cullCamera);
	bool edit();

	void setResolution(int _resolutionX, int _resolutionY);

	void setFlag(Flag _flag, bool _value);
	bool getFlag(Flag _flag) const         { return flags.get(_flag); }

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

	enum Pass_
	{
		Pass_Shadow,
		Pass_GBuffer,
		Pass_Scene,
		Pass_Wireframe,
		Pass_Final,

		Pass_Count
	};
	typedef uint64 Pass;

	std::function<void(Pass _pass, const Camera& _camera)> drawCallback;

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
	Shader*         shBloomDownsample          = nullptr;
	Shader*         shBloomUpsample            = nullptr;

	bool            pauseUpdate                = false;
	
	struct Settings
	{
		ivec2 resolution                = ivec2(-1);
		int   minShadowMapResolution    = 128;
		int   maxShadowMapResolution    = 4096;
		bool  enableCulling             = true;
		bool  cullBySubmesh             = true;
		float motionBlurTargetFps       = 60.0f;
		int   motionBlurTileWidth       = 20;
		int   motionBlurQuality         = 1;
		float taaSharpen                = 0.4f;
		float bloomScale                = -1.0f;
		float bloomBrightness           = 0.0f;
		int   bloomQuality              = 1;
		float materialTextureAnisotropy = 4.0f;
	};
	Settings settings;
	Flags    flags;

private:

	BasicRenderer(Flags _flags);
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
		bool                        cullBackFace        = true; // \todo Pipeline state flags.
		eastl::vector<DrawInstance> instanceData;

		// \todo This data can be shared between scene/shadow passes - split it out (but need to cull against *all* cameras).
		Buffer*                     bfSkinning          = nullptr;
		eastl::vector<mat4>         skinningData;
	};
	using DrawCallMap = eastl::unordered_map<uint64, DrawCall>;
	DrawCallMap                sceneDrawCalls;
	eastl::vector<DrawCallMap> shadowDrawCalls;
	eastl::vector<void*>       shadowMapAllocations; // \todo encapsule draw call map, camera and shadow allocation

	eastl::vector<BasicRenderableComponent*> culledSceneRenderables;
	eastl::vector<BasicRenderableComponent*> shadowRenderables;
	eastl::vector<BasicLightComponent*>      culledLights;
	eastl::vector<BasicLightComponent*>      culledShadowLights;

	void updateDrawCalls(Camera* _cullCamera);

	void addDrawCall(const BasicRenderableComponent* _renderable, int _submeshIndex, DrawCallMap& map_);
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
		float _pad          = 0.0f;
	};
	eastl::vector<LightInstance> lightInstances;

	struct alignas(16) ShadowLightInstance: public LightInstance
	{
		mat4  worldToShadow = identity;
		vec2  uvBias        = vec2(0.0f);
		float uvScale       = 1.0f;
		float arrayIndex    = 0.0f;

		ShadowLightInstance() {} // \todo without this, shadowLightInstances.push_back() crashes in release builds?
	};
	eastl::vector<ShadowLightInstance> shadowLightInstances;

	struct alignas(16) ImageLightInstance
	{
		float    brightness   = 1.0f;
		bool     isBackground = false;
		Texture* texture      = nullptr;

		ImageLightInstance() {} // \todo without this, imageLightInstances.push_back() crashes in release builds?
	};
	eastl::vector<ImageLightInstance> imageLightInstances;
	void updateImageLightInstances();

	struct alignas(16) PostProcessData
	{
		vec4   bloomWeights    = vec4(0.2f);
		float  motionBlurScale = 0.0f;     // current fps / target fps
		uint32 frameIndex      = 0;
	};
	PostProcessData postProcessData;
	void updatePostProcessData(float _dt, uint32 _frameIndex);

	void updateBuffer(Buffer*& bf_, const char* _name, GLsizei _size, void* _data);

	Texture* txBRDFLut = nullptr;

	void initBRDFLut();
	void shutdownBRDFLut();

}; // class BasicRenderer

} // namespace frm
