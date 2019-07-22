#include "Component.h"

#include <frm/core/BasicMaterial.h>
#include <frm/core/Mesh.h>
#include <frm/core/Scene.h>
#include <frm/core/Texture.h>

#include <apt/log.h>
#include <apt/FileSystem.h>
#include <apt/Serializer.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                  Component

*******************************************************************************/

APT_FACTORY_DEFINE(Component);


/*******************************************************************************

                            Component_BasicRenderable

*******************************************************************************/

APT_FACTORY_REGISTER_DEFAULT(Component, Component_BasicRenderable);

eastl::vector<Component_BasicRenderable*> Component_BasicRenderable::s_instances;

bool Component_BasicRenderable::init()
{
	shutdown();

	bool ret = true;
	
	m_mesh = Mesh::Create(m_meshPath.c_str());
	if (!m_mesh)
	{
		return false;
	}

	m_materials.resize(m_materialPaths.size());
	for (size_t i = 0; i < m_materialPaths.size(); ++i)
	{
		if (m_materialPaths[i].isEmpty())
		{
			m_materials[i] = nullptr;
			continue;
		}
		m_materials[i] = BasicMaterial::Create(m_materialPaths[i].c_str());
		ret &= m_materials[i] != nullptr;
	}

	s_instances.push_back(this);

	return ret;
}

void Component_BasicRenderable::shutdown()
{
	auto it = eastl::find(s_instances.begin(), s_instances.end(), this);
	if (it != s_instances.end())
	{
		s_instances.erase_unsorted(it);
	}

	for (BasicMaterial* material : m_materials)
	{
		BasicMaterial::Release(material);
	}
	Mesh::Release(m_mesh);
}

void Component_BasicRenderable::update(float _dt)
{
	m_prevWorld = m_node->getWorldMatrix();
}

bool Component_BasicRenderable::edit()
{
	bool ret = false;
	ImGui::PushID(this);

	ret |= ImGui::ColorEdit3("Color", &m_colorAlpha.x);
	ret |= ImGui::SliderFloat("Alpha", &m_colorAlpha.w, 0.0f, 1.0f);

	ImGui::Spacing();

	if (ImGui::Button("Mesh"))
	{
		PathStr path = m_meshPath;
		if (FileSystem::PlatformSelect(path, { "*.obj", "*.md5mesh" }))
		{
			path = FileSystem::MakeRelative(path.c_str());
			if (path != m_meshPath)
			{
				Mesh* mesh = Mesh::Create(path.c_str());
				if (mesh)
				{
					Mesh::Release(m_mesh);
					m_mesh = mesh;
					m_meshPath = path;
					ret = true;

					if (m_materialPaths.size() < m_mesh->getSubmeshCount())
					{
						m_materialPaths.resize(m_mesh->getSubmeshCount());
						while (m_materials.size() > m_materialPaths.size())
						{
							BasicMaterial::Release(m_materials.back());
							m_materials.pop_back();
						}
						while (m_materials.size() < m_materialPaths.size())
						{
							m_materials.push_back(nullptr);
						}
					}
				}
			}
		}
	}
	ImGui::SameLine();
	ImGui::Text(m_meshPath.c_str());

	ImGui::Spacing();
	if (ImGui::TreeNode("Materials"))
	{
		for (size_t i = 0; i < m_materialPaths.size(); ++i)
		{
			ImGui::PushID((int)i);
			String<16> label(i == 0 ? "Global" : "Submesh %u", i - 1);
			if (ImGui::Button(label.c_str()))
			{
				PathStr path = m_materialPaths[i];
				if (FileSystem::PlatformSelect(path, { "*.json" }))
				{
					path = FileSystem::MakeRelative(path.c_str());
					if (path != m_materialPaths[i])
					{
						BasicMaterial* material = BasicMaterial::Create(path.c_str());
						if (material)
						{
							BasicMaterial::Release(m_materials[i]);
							m_materials[i] = material;
							m_materialPaths[i] = path;
							ret = true;
						}
					}	
				}
			}
			ImGui::SameLine();
			ImGui::Text(m_materialPaths[i].c_str());
			if (m_materials[i])
			{
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_TIMES "##delete"))
				{
					BasicMaterial::Release(m_materials[i]);
					m_materialPaths[i] = "";
					ret = true;
				}
				// \todo material edit
				//ImGui::SameLine();
				//static bool editMaterial = false;
				//if (ImGui::Button(ICON_FA_EXTERNAL_LINK "##edit"))
				//{
				//	editMaterial = !editMaterial;
				//}
				//if (editMaterial && BasicMaterial::Edit(m_materials[i], &editMaterial))
				//{
				//	m_materialPaths[i] = m_materials[i]->getPath();
				//	ret = true;
				//}
			}
			ImGui::PopID();

		 // if the global material is set don't show the submesh slots
			if (i == 0 && !m_materialPaths[0].isEmpty())
			{
				break;
			}
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
	return ret;
}

bool Component_BasicRenderable::serialize(apt::Serializer& _serializer_)
{
	Serialize(_serializer_, m_colorAlpha, "ColorAlpha");
	Serialize(_serializer_, m_castShadows, "CastShadows");
	Serialize(_serializer_, m_meshPath, "Mesh");
	uint materialCount = m_materialPaths.size();
	if (_serializer_.beginArray(materialCount, "Material"))
	{
		m_materialPaths.resize(materialCount);
		for (uint i = 0; i < m_materialPaths.size(); ++i)
		{
			Serialize(_serializer_, m_materialPaths[i]);
		}

		_serializer_.endArray();
	}

	return _serializer_.getError() == nullptr;
}

/*******************************************************************************

                            Component_BasicLight

*******************************************************************************/

APT_FACTORY_REGISTER_DEFAULT(Component, Component_BasicLight);

eastl::vector<Component_BasicLight*> Component_BasicLight::s_instances;

bool Component_BasicLight::init()
{
	s_instances.push_back(this);

	return true;
}

void Component_BasicLight::shutdown()
{
	auto it = eastl::find(s_instances.begin(), s_instances.end(), this);
	if (it != s_instances.end())
	{
		s_instances.erase_unsorted(it);
	}
}

void Component_BasicLight::update(float _dt)
{
}

bool Component_BasicLight::edit()
{
	bool ret = false;
	ImGui::PushID(this);

	ret |= ImGui::Combo("Type", &m_type, "Direct\0Point\0Spot\0");
	ret |= ImGui::ColorEdit3("Color", &m_colorBrightness.x);
	ret |= ImGui::DragFloat("Brightness", &m_colorBrightness.w);
	if (m_type == Type_Point || m_type == Type_Spot)
	{
		ret |= ImGui::DragFloat2("Linear Attenuation", &m_linearAttenuation.x);
	}
	if (m_type == Type_Spot)
	{
		ret |= ImGui::DragFloat2("Radial Attenuation", &m_linearAttenuation.x);
	}

	ImGui::PopID();

	return ret;
}

bool Component_BasicLight::serialize(apt::Serializer& _serializer_)
{
	const char* kTypeNames[3] = { "Direct", "Point", "Spot" };
	SerializeEnum(_serializer_, m_type, kTypeNames, "Type");
	Serialize(_serializer_, m_colorBrightness, "ColorBrightness");
	Serialize(_serializer_, m_linearAttenuation, "LinearAttenuation");
	Serialize(_serializer_, m_radialAttenuation, "RadialAttenuation");
	return _serializer_.getError() == nullptr;
}
