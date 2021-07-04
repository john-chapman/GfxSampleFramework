#include "PhysicsGeometry.h"
#include "PhysicsInternal.h"
#include "Physics.h"

#include <frm/core/hash.h>
#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Json.h>
#include <frm/core/Mesh.h>
#include <frm/core/Serializer.h>

#include <imgui/imgui.h>

namespace frm {

static const char* kTypeStr[PhysicsGeometry::Type_Count]
{
	"Sphere",       //Type_Sphere,
	"Box",          //Type_Box,
	"Plane",        //Type_Plane,
	"Capsule",      //Type_Capsule,
	"ConvexMesh",   //Type_ConvexMesh,
	"TriangleMesh", //Type_TriangleMesh,
	"Heightfield"   //Type_Heightfield,
};

// PUBLIC

PhysicsGeometry* PhysicsGeometry::Create(const char* _path)
{
	Id id = GetHashId(_path);
	PhysicsGeometry* ret = Find(id);
	if (!ret) 
	{
		ret = FRM_NEW(PhysicsGeometry(id, FileSystem::GetFileName(_path).c_str()));
		ret->m_path.set(_path);
	}
	Use(ret);
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateSphere(float _radius, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsSphere%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_Sphere;
	ret->m_data.sphere.radius = _radius;
	Use(ret);
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateBox(const vec3& _halfExtents, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsBox%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_Box;
	ret->m_data.box.halfExtents = _halfExtents;
	Use(ret);
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateCapsule(float _radius, float _halfHeight, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsCapsule%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_Capsule;
	ret->m_data.capsule.radius = _radius;
	ret->m_data.capsule.halfHeight = _halfHeight;
	Use(ret);
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreatePlane(const vec3& _normal, const vec3& _origin, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsPlane%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_Plane;
	ret->m_data.plane.normal = Normalize(_normal);
	ret->m_data.plane.offset = Dot(_normal, _origin);
	Use(ret);
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateConvexMesh(const char* _path, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsConvexMesh%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_ConvexMesh;
	ret->m_dataPath = _path;
	Use(ret);
	return ret;
}

PhysicsGeometry* PhysicsGeometry::CreateTriangleMesh(const char* _path, const char* _name)
{
	Id id = GetUniqueId();
	String<32> name = _name ? _name : String<32>("PhysicsTrianlgeMesh%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->m_type = Type_TriangleMesh;
	ret->m_dataPath = _path;
	Use(ret);
	return ret;
}

PhysicsGeometry* PhysicsGeometry::Create(Serializer& _serializer_)
{
	Id id = GetUniqueId();
	String<32> name("PhysicsGeometry%llu", id);
	PhysicsGeometry* ret = FRM_NEW(PhysicsGeometry(id, name.c_str()));
	ret->serialize(_serializer_);
	Use(ret);
	return ret;
}

void PhysicsGeometry::Destroy(PhysicsGeometry*& _inst_)
{
	FRM_DELETE(_inst_);
}

bool PhysicsGeometry::Edit(PhysicsGeometry*& _physicsGeom_, bool* _open_)
{
	auto SelectPath = [](PathStr& path_) -> bool
		{
			if (FileSystem::PlatformSelect(path_, { "*.physgeo" }))
			{
				FileSystem::SetExtension(path_, "physgeo");
				path_ = FileSystem::MakeRelative(path_.c_str());
				return true;
			}
			return false;
		};

	bool ret = false;

	String<32> windowTitle = "Physics Geometry Editor";
	if (_physicsGeom_ && !_physicsGeom_->m_path.isEmpty())
	{
		windowTitle.appendf(" -- '%s'", _physicsGeom_->m_path.c_str());
	}
	windowTitle.append("###PhysicsGeometryEditor");

	if (_physicsGeom_ && ImGui::Begin(windowTitle.c_str(), _open_, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New"))
				{			
					Release(_physicsGeom_);
					_physicsGeom_ = CreateBox(vec3(1.0f));
					ret = true;
				}

				if (ImGui::MenuItem("Open.."))
				{
					PathStr newPath;
					if (SelectPath(newPath))
					{
						if (newPath != _physicsGeom_->m_path)
						{
							PhysicsGeometry* newPhysGeom = Create(newPath.c_str());
							if (CheckResource(newPhysGeom))
							{
								Release(_physicsGeom_);
								_physicsGeom_ = newPhysGeom;
								ret = true;
							}
							else
							{
								Release(newPhysGeom);
							}
						}
					}
				}
				
				if (ImGui::MenuItem("Save", nullptr, nullptr, !_physicsGeom_->m_path.isEmpty()))
				{
					if (!_physicsGeom_->m_path.isEmpty())
					{
						Json json;
						SerializerJson serializer(json, SerializerJson::Mode_Write);
						if (_physicsGeom_->serialize(serializer))
						{
							Json::Write(json, _physicsGeom_->m_path.c_str());
						}
					}
				}

				if (ImGui::MenuItem("Save As.."))
				{
					if (SelectPath(_physicsGeom_->m_path))
					{
						Json json;
						SerializerJson serializer(json, SerializerJson::Mode_Write);
						if (_physicsGeom_->serialize(serializer))
						{
							Json::Write(json, _physicsGeom_->m_path.c_str());
						}
						ret = true;
					}
				}
								
				if (ImGui::MenuItem("Reload", nullptr, nullptr, !_physicsGeom_->m_path.isEmpty()))
				{
					_physicsGeom_->reload();
					ret = true;
				}

				ImGui::EndMenu();
			}
			
			ImGui::EndMenuBar();
		}

		ret |= _physicsGeom_->edit();

		ImGui::End();
	}

	// If modified, need to reinit all component instances which use this resource.
	if (ret)
	{
		for (auto component : PhysicsComponent::GetActiveComponents())
		{
			if (component->getGeometry() == _physicsGeom_ && component->getState() == World::State::PostInit)
			{
				FRM_VERIFY(component->reinit());
			}
		}		
	}

	return ret;
}

bool PhysicsGeometry::reload()
{
	shutdownImpl(); // Need to call this first; if the type changes after serialization, a later call to shutdownImpl will crash.

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

	return initImpl();
}

bool PhysicsGeometry::edit()
{
	bool ret = false;
	bool reinit = false;

	ImGui::PushID(this);
	
	/*String<32> buf = m_name;
	if (ImGui::InputText("Name", &buf[0], buf.getCapacity(), ImGuiInputTextFlags_EnterReturnsTrue) && buf[0] != '\0')
	{
		m_name.set(buf.c_str());
		ret = true;
	}*/
	
	Type newType = m_type;
	if (ImGui::Combo("Type", &newType, kTypeStr, Type_Count))
	{
		if (newType != m_type)
		{
			ret = reinit = true;

			switch (newType)
			{
				default:
					break;
				case Type_Sphere:
					m_data.sphere.radius = 0.5f;
					break;
				case Type_Box:
					m_data.box.halfExtents = vec3(0.5f);
					break;
				case Type_Plane:
					m_data.plane.normal = vec3(0.0f, 1.0f, 0.0f);
					m_data.plane.offset = 0.0f;
					break;
				case Type_Capsule:
					m_data.capsule.radius = 0.5f;
					m_data.capsule.halfHeight = 1.0f;
					break;
				case Type_ConvexMesh:
				case Type_TriangleMesh:
				case Type_Heightfield:
					if (!editDataPath())
					{
						newType = m_type;
						ret = reinit = false;
					}
					break;
			};
		}
	}

	switch (m_type)
	{
		default:
			break;
		case Type_Sphere:
			if (ImGui::SliderFloat("Radius", &m_data.sphere.radius, 1e-4f, 16.0f))
			{
				m_data.sphere.radius = Max(m_data.sphere.radius, 1e-4f);
				ret = reinit = true;
			}
			break;
		case Type_Box:
			if (ImGui::SliderFloat3("Half Extents", &m_data.box.halfExtents.x, 1e-4f, 16.0f))
			{
				m_data.box.halfExtents = Max(m_data.box.halfExtents, vec3(1e-4f));
				ret = reinit = true;
			}
			break;
		case Type_Plane:
		{
			// \todo better editor for this?
			bool changed = false;
			changed |= ImGui::SliderFloat3("Normal", &m_data.plane.normal.x, -1.0f, 1.0f); 
			m_data.plane.normal = Normalize(m_data.plane.normal);
			changed |= ImGui::DragFloat("Offset", &m_data.plane.offset);
			if (changed)
			{
				ret = reinit = true;
			}
			break;
		}
		case Type_ConvexMesh:
		case Type_TriangleMesh:
		{			
			if (ImGui::Button("Mesh Data"))
			{
				if (editDataPath())
				{
					ret = reinit = true;
				}
			}
			ImGui::SameLine();
			ImGui::Text(m_dataPath.c_str());
			break;
		}
	};

	if (reinit)
	{	
		shutdownImpl();
		m_type = newType;
		initImpl();
	}

	ImGui::PopID();
	
	return ret;
}

bool PhysicsGeometry::serialize(Serializer& _serializer_)
{
	bool ret = true;

	ret |= SerializeEnum<Type, Type_Count>(_serializer_, m_type, kTypeStr, "Type");

	switch (m_type)
	{
		default:
		{
			FRM_LOG_ERR("PhysicsGeometry::serialize -- Invalid type (%d)", m_type);
			ret = false;
			break;
		}
		case Type_Sphere:
		{
			ret |= Serialize(_serializer_, m_data.sphere.radius, "Radius");
			break;
		}
		case Type_Box:
		{
			ret |= Serialize(_serializer_, m_data.box.halfExtents, "HalfExtents");
			break;
		}
		case Type_Capsule:
		{
			ret |= Serialize(_serializer_, m_data.capsule.radius, "Radius");
			ret |= Serialize(_serializer_, m_data.capsule.halfHeight, "HalfHeight");
			break;
		}
		case Type_Plane:
		{
			ret |= Serialize(_serializer_, m_data.plane.normal, "Normal");
			ret |= Serialize(_serializer_, m_data.plane.offset, "Offset");
			break;
		}
		case Type_ConvexMesh:
		case Type_TriangleMesh:
		{
			ret |= Serialize(_serializer_, m_dataPath, "DataPath");
			break;
		}
	};
	
	Serialize(_serializer_, m_name, "Name"); // optional
	setState(ret ? State_Unloaded : State_Error);

	return ret;
}

PhysicsGeometry::Id PhysicsGeometry::getHash() const
{
	Id ret = 0;
	ret = Hash<Id>(&m_type, sizeof(m_type), ret);
	ret = Hash<Id>(&m_data, sizeof(m_data), ret);
	ret = HashString<Id>(m_name.c_str(), ret);
	return ret;
}

// PRIVATE

PhysicsGeometry::PhysicsGeometry(uint64 _id, const char* _name)
	: Resource<PhysicsGeometry>(_id, _name)
{
}

PhysicsGeometry::~PhysicsGeometry()
{
	shutdownImpl();
}

bool PhysicsGeometry::initImpl()
{
	shutdownImpl();

	File cachedData;
	PathStr cachedPath;
	if (m_type == Type_ConvexMesh || m_type == Type_TriangleMesh)
	{
		FRM_ASSERT(!m_dataPath.isEmpty()); // \todo

		cachedPath.setf("_cache/%s.physx", FileSystem::GetFileName(m_dataPath.c_str()).c_str());
			
		if (FileSystem::Exists(cachedPath.c_str()))
		{
			DateTime sourceDate = FileSystem::GetTimeModified(m_dataPath.c_str());
			DateTime cachedDate = FileSystem::GetTimeModified(cachedPath.c_str());
			if (sourceDate <= cachedDate)
			{
				FRM_LOG("PhysicsGeometry: Loading cached data '%s'", cachedPath.c_str());
				if (!FileSystem::Read(cachedData, cachedPath.c_str()))
				{
					FRM_LOG_ERR("PhysicsGeometry: Error loading cached data '%s'", cachedPath.c_str());
				}
			}
		}
	}

	physx::PxGeometryHolder* geometryUnion = FRM_NEW(physx::PxGeometryHolder);
	switch (m_type)
	{
		default:
			break;
		case Type_Sphere:
			geometryUnion->sphere() = physx::PxSphereGeometry(m_data.sphere.radius);
			break;
		case Type_Box:
			geometryUnion->box() = physx::PxBoxGeometry(Vec3ToPx(m_data.box.halfExtents));
			break;
		case Type_Plane:
			geometryUnion->plane() = physx::PxPlaneGeometry(); // plane geometry requires a local pose on the shape, see Physics:InitImpl().
			break;
		case Type_Capsule:
			geometryUnion->capsule() = physx::PxCapsuleGeometry(m_data.capsule.radius, m_data.capsule.halfHeight);
			break;
		case Type_ConvexMesh:
		{
			if (cachedData.getDataSize() == 0)
			{
				Mesh* mesh = Mesh::Create(m_dataPath.c_str(), Mesh::CreateFlags(0), { "PHYS" });
				physx::PxDefaultMemoryOutputStream pxOutput;
				bool cookStatus = PxCookConvexMesh(*mesh, pxOutput);
				AlignedBox bb = mesh->getBoundingBox();
				Mesh::Destroy(mesh);

				if (!cookStatus)
				{
					setState(State_Error);
					m_type = Type_Box;
					geometryUnion->box() = physx::PxBoxGeometry(Vec3ToPx((bb.m_max - bb.m_min) / 2.0f));
					return true; // \hack Don't crash.
				}

				cachedData.setData((const char*)pxOutput.getData(), pxOutput.getSize());
				FileSystem::Write(cachedData, cachedPath.c_str());
			}

			physx::PxDefaultMemoryInputData pxInput((physx::PxU8*)cachedData.getData(), (physx::PxU32)cachedData.getDataSize());
			geometryUnion->convexMesh() = physx::PxConvexMeshGeometry(g_pxPhysics->createConvexMesh(pxInput));

			break;
		}
		case Type_TriangleMesh:
		{
			if (cachedData.getDataSize() == 0)
			{
				Mesh* mesh = Mesh::Create(m_dataPath.c_str(), Mesh::CreateFlags(0), { "PHYS" });
				physx::PxDefaultMemoryOutputStream pxOutput;
				bool cookStatus = PxCookTriangleMesh(*mesh, pxOutput);
				AlignedBox bb = mesh->getBoundingBox();
				Mesh::Destroy(mesh);

				if (!cookStatus)
				{
					setState(State_Error);
					m_type = Type_Box;
					geometryUnion->box() = physx::PxBoxGeometry(Vec3ToPx((bb.m_max - bb.m_min) / 2.0f));
					return true; // \hack Don't crash.
				}

				cachedData.setData((const char*)pxOutput.getData(), pxOutput.getSize()); // \todo avoid this copy?
				FileSystem::Write(cachedData, cachedPath.c_str());
			}

			physx::PxDefaultMemoryInputData pxInput((physx::PxU8*)cachedData.getData(), (physx::PxU32)cachedData.getDataSize());
			geometryUnion->triangleMesh() = physx::PxTriangleMeshGeometry(g_pxPhysics->createTriangleMesh(pxInput)); // \todo release the tri mesh?

			break;
		}
		case Type_Heightfield:
			FRM_ASSERT(false);
			break;		
	};

	m_impl = geometryUnion;
	
	return true;
}

void PhysicsGeometry::shutdownImpl()
{
	if (m_impl)
	{
		physx::PxGeometryHolder* geometryUnion = (physx::PxGeometryHolder*)m_impl;
		switch (m_type)
		{
			case Type_Sphere:
			case Type_Box:
			case Type_Plane:
			case Type_Capsule:
				break;
			case Type_ConvexMesh:
				if (geometryUnion->convexMesh().convexMesh)
				{
					geometryUnion->convexMesh().convexMesh->release();
				}
				break;
			case Type_TriangleMesh:
				if (geometryUnion->triangleMesh().triangleMesh)
				{
					geometryUnion->triangleMesh().triangleMesh->release();
				}
				break;
			case Type_Heightfield:
			default:
				FRM_ASSERT(false);
				break;	
		}

		FRM_DELETE(geometryUnion);
		m_impl = nullptr;
	}
}

bool PhysicsGeometry::editDataPath()
{
	PathStr dataPath = m_dataPath;
	if (FileSystem::PlatformSelect(dataPath, { "*.obj", "*.gltf" }))
	{
		m_dataPath = FileSystem::MakeRelative(dataPath.c_str());
		return true;
	}

	return false;
}

} // namespace frm
