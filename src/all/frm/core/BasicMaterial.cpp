#include "BasicMaterial.h"

#include <frm/core/Texture.h>

#include <apt/log.h>
#include <apt/memory.h>
#include <apt/FileSystem.h>
#include <apt/Json.h>
#include <apt/Serializer.h>
#include <apt/Time.h>

#include <imgui/imgui.h>

namespace frm {

/*******************************************************************************

                                BasicMaterial

*******************************************************************************/

static constexpr const char* kMapStr[] =
{
	"Albedo",
	"Normal",
	"Rough",
	"Metal",
	"Cavity",
	"Height",
	"Emissive",
};

static constexpr const char* kDefaultMaps[] =
{
	"textures/BasicMaterial/default_albedo.png",
	"textures/BasicMaterial/default_normal.png",
	"textures/BasicMaterial/default_rough.png",
	"textures/BasicMaterial/default_metal.png",
	"textures/BasicMaterial/default_cavity.png",
	"textures/BasicMaterial/default_height.png",
	"textures/BasicMaterial/default_emissive.png",
};


// PUBLIC

BasicMaterial* BasicMaterial::Create()
{
	Id id = GetUniqueId();
	NameStr name("Material%u", id);
	BasicMaterial* ret = APT_NEW(BasicMaterial(id, name.c_str()));
	Use(ret);
	return ret;
}

BasicMaterial* BasicMaterial::Create(const char* _path)
{
	Id id = GetHashId(_path);
	BasicMaterial* ret = Find(id);
	if (!ret) 
	{
		ret = APT_NEW(BasicMaterial(id, apt::FileSystem::StripPath(_path).c_str()));
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
	APT_DELETE(_basicMaterial_);
	_basicMaterial_ = nullptr;
}

bool BasicMaterial::Edit(BasicMaterial*& _basicMaterial_, bool* _open_)
{
	APT_ASSERT(false); // \todo need a better way to call this function - separate window per material?
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
				apt::PathStr path;
				if (apt::FileSystem::PlatformSelect(path))
				{
					apt::FileSystem::SetExtension(path, "json");
					path = apt::FileSystem::MakeRelative(path.c_str());
					_basicMaterial_->m_path = path;
				}
			}

			if (!_basicMaterial_->m_path.isEmpty())
			{
				apt::Json json;
				apt::SerializerJson serializer(json, apt::SerializerJson::Mode_Write);
				if (_basicMaterial_->serialize(serializer))
				{
					APT_ASSERT(false); // \todo this is broken for relative paths which aren't the default root
					apt::Json::Write(json, _basicMaterial_->m_path.c_str());
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

	APT_AUTOTIMER("BasicMaterial::reload(%s)", m_path.c_str());

	apt::File f;
	if (!apt::FileSystem::Read(f, m_path.c_str())) 
	{
		setState(State_Error);
		return false;
	}
	m_path = f.getPath(); // use f.getPath() to include the root - this is required for reload to work correctly

	if (!apt::FileSystem::CompareExtension("json", m_path.c_str()))
	{
		APT_LOG_ERR("BasicMaterial: Invalid file '%s' (expected .json)", apt::FileSystem::StripPath(m_path.c_str()).c_str());
		setState(State_Error);
		return false;		
	}

	apt::Json json;
	if (!apt::Json::Read(json, f))
	{
		setState(State_Error);
		return false;		
	}

	apt::SerializerJson serializer(json, apt::SerializerJson::Mode_Read);
	if (!Serialize(serializer, *this))
	{
		APT_LOG_ERR("BasicMaterial: Error serializing '%s': %s", apt::FileSystem::StripPath(m_path.c_str()).c_str(), serializer.getError());
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

	ret |= ImGui::ColorEdit3 ("Color",     &m_colorAlpha.x);
	ret |= ImGui::SliderFloat("Alpha",     &m_colorAlpha.w, 0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Roughness", &m_rough,        0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Metal",     &m_metal,        0.0f, 1.0f);
	
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Maps"))
	{
		for (int i = 0; i < Map_Count; ++i)
		{
			ImGui::PushID(kMapStr[i]);
			if (ImGui::Button(kMapStr[i]))
			{
				apt::PathStr path = m_mapPaths[i];
				if (apt::FileSystem::PlatformSelect(path, { "*.dds", "*.psd", "*.tga", "*.png" }))
				{
					path = apt::FileSystem::MakeRelative(path.c_str());
					path = apt::FileSystem::StripRoot(path.c_str());
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
		ret |= ImGui::Checkbox("Alpha Test", &m_alphaTest);
		ImGui::TreePop();
	}

	ImGui::PopID();
	return ret;
}

bool BasicMaterial::serialize(apt::Serializer& _serializer_)
{	
	Serialize(_serializer_, m_colorAlpha,  "ColorAlpha");
	Serialize(_serializer_, m_rough,       "Roughn");
	Serialize(_serializer_, m_metal,       "Metal");
	Serialize(_serializer_, m_alphaTest,   "AlphaTest");
	if (_serializer_.beginObject("Maps"))
	{
		for (int i = 0; i < Map_Count; ++i)
		{
			apt::PathStr mapPath = "";
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

void BasicMaterial::setMap(Map _map, const char* _path)
{
	if (!_path || *_path == '\0')
	{
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
	APT_STATIC_ASSERT(APT_ARRAY_COUNT(kMapStr) == Map_Count);
}

BasicMaterial::~BasicMaterial()
{	
	for (int i = 0; i < Map_Count; ++i)
	{
		Texture::Release(m_maps[i]);
	}
}

} // namespace frms