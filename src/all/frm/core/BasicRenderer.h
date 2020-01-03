#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/types.h>
#include <frm/core/RenderNodes.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// BasicRenderer
//
// \todo 
// - Shadow system (gather shadow casting light components, allocate shadow map
//   resolution from an atlas).
////////////////////////////////////////////////////////////////////////////////
struct BasicRenderer
{
	enum Flag_
	{
		Flag_PostProcess,       // Enable default post processor (motion blur, tonemap). If disabled, txFinal must be written manually.
		Flag_WriteToBackBuffer, // Copy txFinal to the back buffer. Disable for custom upsampling/antialiasing.

		Flags_Default = 0
		| (1 << Flag_PostProcess)
		| (1 << Flag_WriteToBackBuffer)
	};
	typedef uint32 Flag;

	static BasicRenderer* Create(int _resolutionX, int _resolutionY, uint32 _flags = Flags_Default);
	static void Destroy(BasicRenderer*& _inst_);

	void draw(Camera* _camera, float _dt);
	bool edit();

	void         setResolution(int _resolutionX, int _resolutionY);
	void         setFlag(Flag _flag, bool _value) { flags = BitfieldSet(flags, (int)_flag, _value); }
	bool         getFlag(Flag _flag) const        { return BitfieldGet(flags, (uint32)_flag); }

	Texture*     txGBuffer0             = nullptr;
	Texture*     txGBufferDepth         = nullptr;
	Framebuffer* fbGBuffer              = nullptr;
	Shader*      shGBuffer              = nullptr;
	Shader*      shStaticVelocity       = nullptr;

	Texture*     txScene                = nullptr;
	Framebuffer* fbScene                = nullptr;
	Shader*      shImageLightBg         = nullptr;
	Shader*      shScene                = nullptr;

	Texture*     txFinal                = nullptr;
	Framebuffer* fbFinal                = nullptr;
	Shader*      shPostProcess          = nullptr;

	Buffer*      bfMaterials            = nullptr;
	Buffer*      bfLights               = nullptr;
	Buffer*      bfImageLights          = nullptr;
	Buffer*      bfPostProcessData	    = nullptr;

	float        motionBlurTargetFps    = 50.0f;
	ivec2        resolution             = ivec2(-1);
	uint32       flags                  = Flags_Default;

private:
	BasicRenderer(int _resolutionX, int _resolutionY, uint32 _flags);
	~BasicRenderer();

	void initRenderTargets();
	void shutdownRenderTargets();

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
