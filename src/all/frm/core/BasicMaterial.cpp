#include "BasicMaterial.h"

#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/FileSystem.h>
#include <frm/core/GlContext.h>
#include <frm/core/Json.h>
#include <frm/core/Serializer.h>
#include <frm/core/Texture.h>
#include <frm/core/Time.h>

#include <imgui/imgui.h>

namespace frm {

/*******************************************************************************

                                BasicMaterial

*******************************************************************************/

static constexpr const char* kMapStr[] =
{
	"BaseColor",
	"Metallic",
	"Roughness",
	"Reflectance",
	"Occlusion",
	"Normal",
	"Height",
	"Emissive",
	"Alpha",
};

static constexpr const char* kDefaultMaps[] =
{
	"textures/BasicMaterial/default_basecolor.png",
	"textures/BasicMaterial/default_metallic.png",
	"textures/BasicMaterial/default_roughness.png",
	"textures/BasicMaterial/default_reflectance.png",
	"textures/BasicMaterial/default_occlusion.png",
	"textures/BasicMaterial/default_normal.png",
	"textures/BasicMaterial/default_height.png",
	"textures/BasicMaterial/default_emissive.png",
	"textures/BasicMaterial/default_alpha.png",
};

static constexpr const char* kFlagStr[] =
{
	"AlphaTest",
};


// PUBLIC

BasicMaterial* BasicMaterial::Create()
{
	Id id = GetUniqueId();
	NameStr name("Material%u", id);
	BasicMaterial* ret = FRM_NEW(BasicMaterial(id, name.c_str()));
	Use(ret);
	return ret;
}

BasicMaterial* BasicMaterial::Create(const char* _path)
{
	Id id = GetHashId(_path);
	BasicMaterial* ret = Find(id);
	if (!ret) 
	{
		ret = FRM_NEW(BasicMaterial(id, FileSystem::StripPath(_path).c_str()));
		ret->m_path.set(_path);
	}
	Use(ret);
	if (ret->getState() != State_Loaded) 
	{
	 // \todo replace with default?
	}
	return ret;
}

void BasicMaterial::Destroy(BasicMaterial*& _basicMaterial_)
{
	FRM_DELETE(_basicMaterial_);
	_basicMaterial_ = nullptr;
}

bool BasicMaterial::Edit(BasicMaterial*& _basicMaterial_, bool* _open_)
{
	FRM_ASSERT(false); // \todo need a better way to call this function - separate window per material?
	bool ret = false;
	if (_basicMaterial_ && ImGui::Begin("Basic Material", _open_))
	{
		if (ImGui::Button("New"))
		{
			_basicMaterial_ = Create();
		}
		ImGui::SameLine();
		if (ImGui::Button("Save"))
		{
			if (_basicMaterial_->m_path.isEmpty())
			{
				PathStr path;
				if (FileSystem::PlatformSelect(path))
				{
					FileSystem::SetExtension(path, "json");
					path = FileSystem::MakeRelative(path.c_str());
					_basicMaterial_->m_path = path;
				}
			}

			if (!_basicMaterial_->m_path.isEmpty())
			{
				Json json;
				SerializerJson serializer(json, SerializerJson::Mode_Write);
				if (_basicMaterial_->serialize(serializer))
				{
					FRM_ASSERT(false); // \todo this is broken for relative paths which aren't the default root
					Json::Write(json, _basicMaterial_->m_path.c_str());
				}
			}
		}
		ret |= _basicMaterial_->edit();
		ImGui::End();
	}
	return ret;
}

bool BasicMaterial::reload()
{
	if (m_path.isEmpty())
	{
		return true;
	}

	FRM_AUTOTIMER("BasicMaterial::reload(%s)", m_path.c_str());

	File f;
	if (!FileSystem::Read(f, m_path.c_str())) 
	{
		setState(State_Error);
		return false;
	}
	m_path = f.getPath(); // use f.getPath() to include the root - this is required for reload to work correctly

	if (!FileSystem::CompareExtension("json", m_path.c_str()))
	{
		FRM_LOG_ERR("BasicMaterial: Invalid file '%s' (expected .json)", FileSystem::StripPath(m_path.c_str()).c_str());
		setState(State_Error);
		return false;		
	}

	Json json;
	if (!Json::Read(json, f))
	{
		setState(State_Error);
		return false;		
	}

	SerializerJson serializer(json, SerializerJson::Mode_Read);
	if (!Serialize(serializer, *this))
	{
		FRM_LOG_ERR("BasicMaterial: Error serializing '%s': %s", FileSystem::StripPath(m_path.c_str()).c_str(), serializer.getError());
		setState(State_Error);
		return false;
	}

	setState(State_Loaded);
	return true;
}

bool BasicMaterial::edit()
{
	bool ret = false;
	ImGui::PushID(this);

	ret |= ImGui::ColorEdit3 ("Base Color",  &m_baseColor.x);
	ret |= ImGui::SliderFloat("Alpha",       &m_alpha,       0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Metallic",    &m_metallic,    0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Roughness",   &m_roughness,   0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Reflectance", &m_reflectance, 0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Height",      &m_height,      0.0f, 1.0f);
	
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Maps"))
	{
		for (int i = 0; i < Map_Count; ++i)
		{
			ImGui::PushID(kMapStr[i]);
			if (ImGui::Button(kMapStr[i]))
			{
				PathStr path = m_mapPaths[i];
				if (FileSystem::PlatformSelect(path, { "*.dds", "*.psd", "*.tga", "*.png" }))
				{
					path = FileSystem::MakeRelative(path.c_str());
					path = FileSystem::StripRoot(path.c_str());
					setMap(i, path.c_str());
				}
			}
			ImGui::SameLine();
			ImGui::Text("'%s'", m_mapPaths[i].c_str());
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TIMES))
			{
				setMap(i, nullptr);
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Flags"))
	{
		bool alphaTest = BitfieldGet(m_flags, (uint32)Flag_AlphaTest);
		ret |= ImGui::Checkbox("Alpha Test", &alphaTest);
		m_flags = BitfieldSet(m_flags, (uint32)Flag_AlphaTest, alphaTest);
		ImGui::TreePop();
	}

	ImGui::PopID();
	return ret;
}

bool BasicMaterial::serialize(Serializer& _serializer_)
{	
	Serialize(_serializer_, m_baseColor,     "BaseColor");
	Serialize(_serializer_, m_emissiveColor, "EmissiveColor");
	Serialize(_serializer_, m_alpha,         "Alpha");
	Serialize(_serializer_, m_metallic,      "Metallic");
	Serialize(_serializer_, m_roughness,     "Roughness");
	Serialize(_serializer_, m_reflectance,   "Reflectance");
	Serialize(_serializer_, m_height,        "Height");
	
	if (_serializer_.beginObject("Flags"))
	{
		
		for (int i = 0; i < Flag_Count; ++i)
		{
			bool value = BitfieldGet(m_flags, (uint32)i);
			Serialize(_serializer_, value, kFlagStr[i]);
			m_flags = BitfieldSet(m_flags, (uint32)i, value);
		}
		_serializer_.endObject();
	}

	if (_serializer_.beginObject("Maps"))
	{
		for (int i = 0; i < Map_Count; ++i)
		{
			PathStr mapPath = "";
			Serialize(_serializer_, mapPath, kMapStr[i]);
			setMap(i, mapPath.c_str());
		}
		_serializer_.endObject();
	}
	else
	{
		for (int i = 0; i < Map_Count; ++i)
		{
			setMap(i, "");
		}
	}
	return true;
}

void BasicMaterial::bind() const
{
	GlContext* ctx = GlContext::GetCurrent();

	String<32> mapName;
	for (int i = 0; i < Map_Count; ++i)
	{
		mapName.setf("uMaps[%d]", i);
		ctx->bindTexture(mapName.c_str(), m_maps[i]);
	}
}

void BasicMaterial::setMap(Map _map, const char* _path)
{
	if (!_path || *_path == '\0')
	{
		FRM_ASSERT(kDefaultMaps[_map]);
		setMap(_map, kDefaultMaps[_map]);
		return;
	}

	if (m_mapPaths[_map] != _path)
	{
		Texture* tx = Texture::Create(_path);
		if (tx)
		{
			Texture::Release(m_maps[_map]);
			m_maps[_map] = tx;
			m_mapPaths[_map] = _path;
		}
	}
}

// PROTECTED

BasicMaterial::BasicMaterial(Id _id, const char* _name)
	: Resource(_id, _name)
{
	m_index = GetInstanceCount() - 1;
	FRM_STATIC_ASSERT(FRM_ARRAY_COUNT(kMapStr) == Map_Count);
}

BasicMaterial::~BasicMaterial()
{	
	for (int i = 0; i < Map_Count; ++i)
	{
		Texture::Release(m_maps[i]);
	}
}

} // namespace frms