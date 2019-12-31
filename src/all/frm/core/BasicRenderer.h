#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
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
	static BasicRenderer* Create(int _resolutionX, int _resolutionY);
	static void Destroy(BasicRenderer*& _inst_);

	void draw(Camera* _camera, float _dt);
	bool edit();

	Texture*     txGBuffer0             = nullptr;
	Texture*     txGBufferDepth         = nullptr;
	Framebuffer* fbGBuffer              = nullptr;
	Shader*      shGBuffer              = nullptr;

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

private:
	BasicRenderer(int _resolutionX, int _resolutionY);
	~BasicRenderer();

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
		mat4  motionBlurCurrentToPrevious = identity; // previous view-proj * current inverse view-proj
		float motionBlurScale             = 0.0f;     // current fps / target fps
	};
	PostProcessData postProcessData;

 	LuminanceMeter luminanceMeter;

}; // class BasicRenderer

} // namespace frm
