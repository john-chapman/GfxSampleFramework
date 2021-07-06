#include "PhysicsMaterial.h"
#include "PhysicsInternal.h"

#include <frm/core/memory.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Json.h>
#include <frm/core/Serializer.h>

#include <imgui/imgui.h>

using namespace frm;

// PUBLIC

PhysicsMaterial* PhysicsMaterial::Create(const char* _path)
{
	Id id = GetHashId(_path);
	PhysicsMaterial* ret = Find(id);
	if (!ret) 
	{
		ret = FRM_NEW(PhysicsMaterial(id, FileSystem::GetFileName(_path).c_str()));
		ret->m_path.set(_path);
	}
	Use(ret);
	return ret;
}

PhysicsMaterial* PhysicsMaterial::Create(float _staticFriction, float _dynamicFriction, float _restitution, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsMaterial%llu", id);
	PhysicsMaterial* ret = FRM_NEW(PhysicsMaterial(id, name.c_str()));
	Use(ret);
	return ret;
}

PhysicsMaterial* PhysicsMaterial::Create(Serializer& _serializer_)
{
	Id id = GetUniqueId();
	String<32> name = String<32>("PhysicsMaterial%llu", id);
	PhysicsMaterial* ret = FRM_NEW(PhysicsMaterial(id, name.c_str()));
	ret->serialize(_serializer_);
	Use(ret);
	return ret;
}

void PhysicsMaterial::Destroy(PhysicsMaterial*& _inst_)
{
	FRM_DELETE(_inst_);
}

bool PhysicsMaterial::Edit(PhysicsMaterial*& _material_, bool* _open_)
{
	auto SelectPath = [](PathStr& path_) -> bool
		{
			if (FileSystem::PlatformSelect(path_, { "*.physmat" }))
			{
				FileSystem::SetExtension(path_, "physmat");
				path_ = FileSystem::MakeRelative(path_.c_str());
				return true;
			}
			return false;
		};

	bool ret = false;

	String<32> windowTitle = "Physics Material Editor";
	if (_material_ && !_material_->m_path.isEmpty())
	{
		windowTitle.appendf(" -- '%s'", _material_->m_path.c_str());
	}
	windowTitle.append("###PhysicsMaterialEditor");

	if (_material_ && ImGui::Begin(windowTitle.c_str(), _open_, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New"))
				{			
					Release(_material_);
					_material_ = Create(0.5f, 0.5f, 0.2f);
					ret = true;
				}

				if (ImGui::MenuItem("Open.."))
				{
					PathStr newPath;
					if (SelectPath(newPath))
					{
						if (newPath != _material_->m_path)
						{
							PhysicsMaterial* newMaterial = Create(newPath.c_str());
							if (CheckResource(newMaterial))
							{
								Release(_material_);
								_material_ = newMaterial;
								ret = true;
							}
							else
							{
								Release(newMaterial);
							}
						}
					}
				}
				
				if (ImGui::MenuItem("Save", nullptr, nullptr, !_material_->m_path.isEmpty()))
				{
					if (!_material_->m_path.isEmpty())
					{
						Json json;
						SerializerJson serializer(json, SerializerJson::Mode_Write);
						if (_material_->serialize(serializer))
						{
							Json::Write(json, _material_->m_path.c_str());
						}
					}
				}

				if (ImGui::MenuItem("Save As.."))
				{
					if (SelectPath(_material_->m_path))
					{
						Json json;
						SerializerJson serializer(json, SerializerJson::Mode_Write);
						if (_material_->serialize(serializer))
						{
							Json::Write(json, _material_->m_path.c_str());
						}
						ret = true;
					}
				}
								
				if (ImGui::MenuItem("Reload", nullptr, nullptr, !_material_->m_path.isEmpty()))
				{
					_material_->reload();
					ret = true;
				}

				ImGui::EndMenu();
			}
			
			ImGui::EndMenuBar();
		}

		ret |= _material_->edit();

		ImGui::End();
	}

	// If modified, need to reinit all component instances which use this resource.
	if (ret)
	{
		// \hack Component shutdown may end up destroying this resource if it's the only reference, need to keep alive.
		PhysicsMaterial* resPtr = _material_;
		Use(resPtr);	
		for (auto component : PhysicsComponent::GetActiveComponents())
		{
			if (component->getMaterial() == resPtr && component->getState() == World::State::PostInit)
			{
				FRM_VERIFY(component->reinit());

			}
		}		
		Release(resPtr); // \hack See above.
	}

	return ret;
}

bool PhysicsMaterial::reload()
{
	if (!m_path.isEmpty())
	{
		File file;
		if (!FileSystem::Read(file, m_path.c_str()))
		{
			return false;
		}
		m_path = file.getPath(); // use f.getPath() to include the root - this is required for reload to work correctly
	
		Json json;
		if (!Json::Read(json, file))
		{
			return false;
		}
	
		SerializerJson serializer(json, SerializerJson::Mode_Read);
		if (!serialize(serializer))
		{
			return false;
		}
	}

	updateImpl();

	setState(State_Loaded);
	
	return true;
}

bool PhysicsMaterial::edit()
{
	bool ret = false;

	ImGui::PushID(this);
	/*String<32> buf = m_name;
	if (ImGui::InputText("Name", &buf[0], buf.getCapacity(), ImGuiInputTextFlags_EnterReturnsTrue) && buf[0] != '\0')
	{
		m_name.set(buf.c_str());
		ret = true;
	}*/
	ret |= ImGui::SliderFloat("Static Friction", &m_staticFriction, 0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Dynamic Friction", &m_dynamicFriction, 0.0f, 1.0f);
	ret |= ImGui::SliderFloat("Restitution", &m_restitution, 0.0f, 1.0f);

	ImGui::Spacing();
	bool save = false;
	if (!m_path.isEmpty())
	{
		if (ImGui::Button("Save"))
		{
			save = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload"))
		{
			reload();
			ret = false;
		}
		ImGui::SameLine();
	}
	ImGui::PopID();

	m_staticFriction = Max(m_staticFriction, m_dynamicFriction); // matches behavior of PxMaterial

	if (ret)
	{
		updateImpl();
	}
	return ret;
}

bool PhysicsMaterial::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_staticFriction,  "m_staticFriction");
	ret &= Serialize(_serializer_, m_dynamicFriction, "m_dynamicFriction");
	ret &= Serialize(_serializer_, m_restitution,     "m_restitution");
	       Serialize(_serializer_, m_name,            "m_name"); // optional
	setState(ret ? State_Unloaded : State_Error);
	return ret;
}

// PRIVATE

PhysicsMaterial::PhysicsMaterial(uint64 _id, const char* _name)
	: Resource<PhysicsMaterial>(_id, _name)
{
}

PhysicsMaterial::~PhysicsMaterial()
{
	if (m_impl)
	{
		((physx::PxMaterial*)m_impl)->release();
		m_impl = nullptr;
	}
}

void PhysicsMaterial::updateImpl()
{
	if (!m_impl)
	{
		PhysicsWorld* physicsWorld = Physics::GetCurrentWorld();
		FRM_ASSERT(physicsWorld);
		PhysicsWorld::Impl* px = physicsWorld->m_impl;

		m_impl = g_pxPhysics->createMaterial(m_staticFriction, m_dynamicFriction, m_restitution);
		physx::PxMaterial* pxMaterial = (physx::PxMaterial*)m_impl;
		pxMaterial->userData = this;
	}

	physx::PxMaterial* pxMaterial = (physx::PxMaterial*)m_impl;
	pxMaterial->setRestitution(m_restitution);
	pxMaterial->setDynamicFriction(m_dynamicFriction);
	pxMaterial->setStaticFriction(m_staticFriction);
}
