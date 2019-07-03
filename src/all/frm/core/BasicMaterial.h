#pragma once

#include <frm/core/def.h>
#include <frm/core/math.h>
#include <frm/core/Resource.h>

#include <apt/FileSystem.h>

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
		Map_Albedo,
		Map_Normal,
		Map_Roughness,
		Map_Cavity,
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
	bool                  serialize(apt::Serializer& _serializer_);

	friend bool Serialize(apt::Serializer& _serializer_, BasicMaterial& _basicMaterial_) { return _basicMaterial_.serialize(_serializer_); }

	int                   getIndex() const           { return m_index; }
	const char*           getPath() const            { return m_path.c_str(); }
	Texture*              getMap(Map _mapName) const { return m_maps[_mapName]; }
	const vec4&           getColorAlpha() const      { return m_colorAlpha; }
	float                 getRoughness() const       { return m_roughness; }
	
	void                  setMap(Map, const char* _path);

protected:

	BasicMaterial(Id _id, const char* _name);
	~BasicMaterial();

	int                                   m_index        = -1; // global index (see BasicRenderer)
	apt::PathStr                          m_path         = "";
	eastl::array<Texture*, Map_Count>     m_maps         = { nullptr };
	eastl::array<apt::PathStr, Map_Count> m_mapPaths     = { "" };
	vec4                                  m_colorAlpha   = vec4(1.0f);
	float                                 m_roughness    = 1.0f;
	bool                                  m_alphaTest    = false;

}; // class BasicMaterial

} // namespace frm

