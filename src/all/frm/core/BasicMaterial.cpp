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

static constexpr const char* kMapStr[BasicMaterial::Map_Count] =
{
	"Albedo",
	"Normal",
	"Rough",
	"Cavity",
	"Height",
};


// PUBLIC

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
	 // \todo replace with default
	}
	return ret;
}

void BasicMaterial::Destroy(BasicMaterial*& _basicMaterial_)
{
	APT_DELETE(_basicMaterial_);
	_basicMaterial_ = nullptr;
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

	releaseMaps();
	initMaps();
}

bool BasicMaterial::edit()
{
	bool ret = false;
	ImGui::PushID(this);

	ret |= ImGui::ColorEdit4("Color, Alpha", &m_colorAlpha.x);
	ret |= ImGui::SliderFloat("Rough", &m_rough, 0.0f, 1.0f);
	
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
					if (path != m_mapPaths[i])
					{
						Texture* tx = Texture::Create(path.c_str());
						if (tx)
						{
							Texture::Release(m_maps[i]);
							m_maps[i] = tx;
							m_mapPaths[i] = path;
						}
					}
				}
			}
			ImGui::SameLine();
			ImGui::Text("'%s'", m_mapPaths[i].c_str());
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TIMES))
			{
				m_mapPaths[i] = "";
				Texture::Release(m_maps[i]);
				m_maps[i] = nullptr; // \todo Texture::Release() doesn't nullify the ptr?
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

bool BasicMaterial::serialize(apt::Serializer&_serializer_)
{	
	Serialize(_serializer_, m_colorAlpha,  "m_colorAlpha");
	Serialize(_serializer_, m_rough,       "m_rough");
	Serialize(_serializer_, m_alphaTest,   "m_alphaTest");
	if (_serializer_.beginArray("m_mapPaths"))
	{
		for (int i = 0; i < Map_Count; ++i)
		{
			Serialize(_serializer_, m_mapPaths[i], kMapStr[i]);
		}
		_serializer_.endArray();
	}
	return true;
}


// PROTECTED

BasicMaterial::BasicMaterial(Id _id, const char* _name)
	: Resource(_id, _name)
{
	APT_STATIC_ASSERT(APT_ARRAY_COUNT(kMapStr) == Map_Count);
}

BasicMaterial::~BasicMaterial()
{
	releaseMaps();
}

bool BasicMaterial::initMaps()
{	
	bool ret = true;
	for (int i = 0; i < Map_Count; ++i)
	{
		if (m_mapPaths[i].isEmpty())
		{
			continue;
		}
		m_maps[i] = Texture::Create(m_mapPaths[i].c_str());
		ret &= m_maps[i] != nullptr;
	}
	return ret;
}

void BasicMaterial::releaseMaps()
{	
	for (int i = 0; i < Map_Count; ++i)
	{
		Texture::Release(m_maps[i]);
	}
}


} // namespace frms