#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/RenderNodes.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// BasicRenderer
////////////////////////////////////////////////////////////////////////////////
struct BasicRenderer
{
	static BasicRenderer* Create(int _resolutionX, int _resolutionY);
	static void Destroy(BasicRenderer*& _inst_);

	void draw(Camera* _camera, float _dt);
	bool edit();

	Texture*     txGBuffer0     = nullptr;
	Texture*     txGBufferDepth = nullptr;
	Framebuffer* fbGBuffer      = nullptr;
	Shader*      shGBuffer      = nullptr;
	Texture*     txScene        = nullptr;
	Framebuffer* fbScene        = nullptr;
	Shader*      shScene        = nullptr;
	Buffer*      bfMaterials    = nullptr;
	Buffer*      bfLights       = nullptr;

private:
	BasicRenderer(int _resolutionX, int _resolutionY);
	~BasicRenderer();

	struct MaterialInstance
	{
		vec4  baseColorAlpha = vec4(1.0f);
		vec4  emissiveColor  = vec4(0.0f);
		float metallic       = 1.0f;
		float roughness      = 1.0f;
		float reflectance    = 1.0f;
		float height         = 1.0f;
	};
	eastl::vector<MaterialInstance> materialInstances;
	void updateMaterialInstances();

	struct DrawInstance
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

	struct LightInstance
	{
	 // \todo pack
		vec4 position      = vec4(0.0f); // A = type.
		vec4 direction     = vec4(0.0f);
		vec4 color         = vec4(0.0f); // RGB = color * brightness, A = brightness
		vec4 attenuation   = vec4(0.0f); // X,Y = linear attenuation start,stop, Z,W = radial attenuation start,stop
	};
	eastl::vector<LightInstance> lightInstances;
	void updateLightInstances(const Camera* _camera);

 	//LuminanceMeter  m_luminanceMeter;
	ColorCorrection m_colorCorrection;

}; // class BasicRenderer

} // namespace frm
