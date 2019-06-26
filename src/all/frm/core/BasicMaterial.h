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
		Map_Rough,
		Map_Cavity,
		Map_Height,
		Map_Emissive,

		Map_Count
	};
	typedef int Map;

	static BasicMaterial* Create(const char* _path);
	static void           Destroy(BasicMaterial*& _basicMaterial_);

	bool                  load()                     { return reload(); }
	bool                  reload();
	bool                  edit();
	bool                  serialize(apt::Serializer& _serializer_);

	friend bool Serialize(apt::Serializer& _serializer_, BasicMaterial& _basicMaterial_) { return _basicMaterial_.serialize(_serializer_); }

	Texture*              getMap(Map _mapName) const { return m_maps[_mapName]; }
	const vec4&           getColorAlpha() const      { return m_colorAlpha; }
	float                 getRough() const           { return m_rough; }

protected:

	BasicMaterial(Id _id, const char* _name);
	~BasicMaterial();

	apt::PathStr                          m_path         = "";
	eastl::array<Texture*, Map_Count>     m_maps         = { nullptr };
	eastl::array<apt::PathStr, Map_Count> m_mapPaths     = { "" };
	vec4                                  m_colorAlpha   = vec4(1.0f);
	float                                 m_rough        = 1.0f;
	bool                                  m_alphaTest    = false;

	bool initMaps();
	void releaseMaps();

}; // class BasicMaterial

} // namespace frm

