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
	"Translucency"
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
	"textures/BasicMaterial/default_translucency.png"
};

static constexpr const char* kDefaultSuffix[] =
{
	"_basecolor",
	"_metallic",
	"_roughness",
	"_reflectance",
	"_occlusion",
	"_normal",
	"_height",
	"_emissive",
	"_alpha",
	"_translucency"
};

static constexpr const char* kFlagStr[] =
{
	"Flip V",
	"Normal Map BC5",
	"Alpha Test",
	"Alpha Dither",	
	"Thin Translucency"
};


// PUBLIC

BasicMaterial* BasicMaterial::Create()
{
	Id id = GetUniqueId();
	NameStr name("Material%u", id);
	BasicMaterial* ret = FRM_NEW(BasicMaterial(id, name.c_str()));
	for (int i = 0; i < Map_Count; ++i)
	{
		ret->setMap(i, ""); // set default maps
	}
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
	auto SelectMaterialPath = [](PathStr& path_) -> bool
		{
			if (FileSystem::PlatformSelect(path_, { "*.mat" }))
			{
				FileSystem::SetExtension(path_, "mat");
				path_ = FileSystem::MakeRelative(path_.c_str());
				return true;
			}
			return false;
		};

	bool ret = false;

	String<32> windowTitle = "Basic Material Editor";
	if (_basicMaterial_ && !_basicMaterial_->m_path.isEmpty())
	{
		windowTitle.appendf(" -- '%s'", _basicMaterial_->m_path.c_str());
	}
	windowTitle.append("###BasicMaterialEditor");

	if (_basicMaterial_ && ImGui::Begin(windowTitle.c_str(), _open_, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New"))
				{			
					Release(_basicMaterial_);
					_basicMaterial_ = Create();
					ret = true;
				}

				if (ImGui::MenuItem("Open.."))
				{
					PathStr newPath;
					if (SelectMaterialPath(newPath))
					{
						if (newPath != _basicMaterial_->m_path)
						{
							BasicMaterial* newMaterial = Create(newPath.c_str());
							if (CheckResource(newMaterial))
							{
								Release(_basicMaterial_);
								_basicMaterial_ = newMaterial;
								ret = true;
							}
							else
							{
								Release(newMaterial);
							}
						}
					}
				}
				
				if (ImGui::MenuItem("Save", nullptr, nullptr, !_basicMaterial_->m_path.isEmpty()))
				{
					if (!_basicMaterial_->m_path.isEmpty())
					{
						Json json;
						SerializerJson serializer(json, SerializerJson::Mode_Write);
						if (_basicMaterial_->serialize(serializer))
						{
							Json::Write(json, _basicMaterial_->m_path.c_str());
						}
					}
				}

				if (ImGui::MenuItem("Save As.."))
				{
					if (SelectMaterialPath(_basicMaterial_->m_path))
					{
						Json json;
						SerializerJson serializer(json, SerializerJson::Mode_Write);
						if (_basicMaterial_->serialize(serializer))
						{
							Json::Write(json, _basicMaterial_->m_path.c_str());
						}
						ret = true;
					}
				}

				if (ImGui::MenuItem("Reload", nullptr, nullptr, !_basicMaterial_->m_path.isEmpty()))
				{
					_basicMaterial_->reload();
				}

				ImGui::EndMenu();
			}
			
			ImGui::EndMenuBar();
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

	if (!FileSystem::CompareExtension("mat", m_path.c_str()))
	{
		FRM_LOG_ERR("BasicMaterial: Invalid file '%s' (expected .mat)", FileSystem::StripPath(m_path.c_str()).c_str());
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
	struct EditorState
	{
		bool mapAutoSelect = true; // Auto-select maps in the same location based on prefix (_basecolor, _normal, etc.).
	};
	static EditorState s_editorState;

	bool ret = false;
	ImGui::PushID(this);

	ret |= ImGui::ColorEdit3 ("Base Color",  &m_baseColor.x);
	ret |= ImGui::SliderFloat("Alpha",       &m_alpha,       0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Metallic",    &m_metallic,    0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Roughness",   &m_roughness,   0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Reflectance", &m_reflectance, 0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Height",      &m_height,      0.0f, 4.0f);
	
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Maps"))
	{
		ImGui::Checkbox("Auto Select", &s_editorState.mapAutoSelect);
		ImGui::Spacing();

		for (int i = 0; i < Map_Count; ++i)
		{
			ImGui::PushID(kMapStr[i]);

			if (m_maps[i])
			{
				TextureView* txView = m_maps[i]->getTextureView();
				ImGui::ImageButton((ImTextureID)txView, ImVec2(128,128), ImVec2(0, 1), ImVec2(1, 0), 1, ImColor(0.5f, 0.5f, 0.5f));
			}
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TIMES))
			{
				setMap(i, nullptr);
			}
			ImGui::SameLine();
			
			if (ImGui::Button(kMapStr[i]))
			{
				PathStr path = m_mapPaths[i];
				if (FileSystem::PlatformSelect(path, { "*.dds", "*.psd", "*.tga", "*.png", "*.jpg" }))
				{
					path = FileSystem::MakeRelative(path.c_str());
					setMap(i, path.c_str());

					// Automatically load textures with the same base name from the same location.
					if (s_editorState.mapAutoSelect)
					{
						PathStr baseName = FileSystem::GetFileName(path.c_str());
						PathStr basePath = FileSystem::GetPath(path.c_str());
						PathStr baseExt  = FileSystem::GetExtension(path.c_str());
						const char* suffixPos = baseName.find(kDefaultSuffix[i]);
						if (suffixPos)
						{
							// Trim baseName to remove suffix.
							baseName.setLength(suffixPos - baseName.begin());

							// Attempt to load all other maps.
							for (int j = 0; j < Map_Count; ++j)
							{
								if (j != i)
								{
									PathStr autoPath = PathStr("%s%s%s.%s", basePath.c_str(), baseName.c_str(), kDefaultSuffix[j], baseExt.c_str()).c_str();
									if (FileSystem::Exists(autoPath.c_str())) // \hack Avoid spamming the error log if the path doesn't exist.
									{
										setMap(j, autoPath.c_str());
									}
								}
							}
						}
					}
				}
			}
			ImGui::SameLine();
			ImGui::Text("'%s'", m_mapPaths[i].c_str());
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Flags"))
	{
		for (int i = 0; i < Flag_Count; ++i)
		{
			bool value = BitfieldGet(m_flags, (uint32)i);
			ret |= ImGui::Checkbox(kFlagStr[i], &value);
			m_flags = BitfieldSet(m_flags, (uint32)i, value);
		}

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
			PathStr mapPath = m_mapPaths[i];
			if (_serializer_.getMode() == Serializer::Mode_Read)
			{
				if (Serialize(_serializer_, mapPath, kMapStr[i]))
				{
					setMap(i, mapPath.c_str());
				}
				else
				{
					setMap(i, kDefaultMaps[i]);
				}
			}
			else if (mapPath != kDefaultMaps[i])
			{
				Serialize(_serializer_, mapPath, kMapStr[i]);
			}
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

void BasicMaterial::bind(GlContext* _ctx_, TextureSampler* _sampler) const
{
	static String<32> mapBindingNames[Map_Count];
	FRM_ONCE
	{
		for (int i = 0; i < Map_Count; ++i)
		{
			mapBindingNames[i].setf("uBasicMaterial_Maps[%d]", i);
		}
	}

	for (int i = 0; i < Map_Count; ++i)
	{
		_ctx_->bindTexture(mapBindingNames[i].c_str(), m_maps[i], _sampler);
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
		if (CheckResource(tx))
		{
			Texture::Release(m_maps[_map]);
			m_maps[_map] = tx;
			m_mapPaths[_map] = _path;

			if (_map == Map_Normal)
			{
				if (tx->getFormat() == GL_COMPRESSED_RG_RGTC2)
				{
					m_flags = BitfieldSet(m_flags, Flag_NormalMapBC5, true);
				}
				else
				{
					m_flags = BitfieldSet(m_flags, Flag_NormalMapBC5, false);
				}
			}
		}
	}
}

// PROTECTED

BasicMaterial::BasicMaterial(Id _id, const char* _name)
	: Resource(_id, _name)
{
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