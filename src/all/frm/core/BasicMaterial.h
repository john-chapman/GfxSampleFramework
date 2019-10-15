#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Resource.h>

#include <EASTL/array.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// BasicMaterial
////////////////////////////////////////////////////////////////////////////////
class BasicMaterial: public Resource<BasicMaterial>
{
public:
	enum Map_
	{
		Map_BaseColor,
		Map_Metallic,
		Map_Roughness,
		Map_Reflectance,
		Map_Occlusion,
		Map_Normal,
		Map_Height,
		Map_Emissive,

		Map_Count
	};
	typedef int Map;

	static BasicMaterial* Create();
	static BasicMaterial* Create(const char* _path);
	static void           Destroy(BasicMaterial*& _basicMaterial_);
	static bool           Edit(BasicMaterial*& _basicMaterial_, bool* _open_ = nullptr);

	bool                  load()                     { return reload(); }
	bool                  reload();
	bool                  edit();
	bool                  serialize(frm::Serializer& _serializer_);

	friend bool Serialize(frm::Serializer& _serializer_, BasicMaterial& _basicMaterial_) { return _basicMaterial_.serialize(_serializer_); }

	int                   getIndex() const           { return m_index; }
	const char*           getPath() const            { return m_path.c_str(); }
	Texture*              getMap(Map _mapName) const { return m_maps[_mapName]; }
	const vec3&           getBaseColor() const       { return m_baseColor; }
	const vec3&           getEmissiveColor() const   { return m_emissiveColor; }
	float                 getAlpha() const           { return m_alpha; }
	float                 getMetallic() const        { return m_metallic; }
	float                 getRoughness() const       { return m_roughness; }
	float                 getReflectance() const     { return m_reflectance; }
	float                 getHeight() const          { return m_height; }
	
	void                  setMap(Map, const char* _path);

protected:

	BasicMaterial(Id _id, const char* _name);
	~BasicMaterial();

	int                                   m_index           = -1; // global index (see BasicRenderer)
	frm::PathStr                          m_path            = "";
	eastl::array<Texture*, Map_Count>     m_maps            = { nullptr };
	eastl::array<frm::PathStr, Map_Count> m_mapPaths        = { "" };
	vec3                                  m_baseColor       = vec3(1.0f);
	vec3                                  m_emissiveColor   = vec3(0.0f);
	float                                 m_alpha           = 1.0f;
	bool                                  m_alphaTest       = false;

	// \todo use textures for everything?
	float                                 m_metallic        = 1.0f;
	float                                 m_roughness       = 1.0f;
	float                                 m_reflectance     = 1.0f;
	float                                 m_height          = 1.0f;

}; // class BasicMaterial

} // namespace frm

