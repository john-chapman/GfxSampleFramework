#include "World.h"

#include <frm/core/Json.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializer.h>
#include <frm/core/Serializable.inl>
#include <frm/core/StringHash.h>
#include <frm/core/world/components/Component.h>
#include <frm/core/world/components/CameraComponent.h>
#include <frm/core/world/components/FreeLookComponent.h>

namespace frm {

/*******************************************************************************

                                 SceneID

*******************************************************************************/

SceneID::SceneID(uint16 _base, uint16 _value)
{
	constexpr uint32 kFnv1aPrime32 = 0x01000193u;
	uint32 tmp = (_base ^ _value) * kFnv1aPrime32;
	value = (tmp >> 16) ^ (((uint32)1u << 16) - 1); // xor-folded 32 bit hash
}

void SceneID::fromString(const char* _str)
{
	value = (uint16)strtoul(_str, nullptr, 16);
}

bool SceneID::serialize(Serializer& _serializer_, const char* _name)
{
	String str;

	if (_serializer_.getMode() == Serializer::Mode_Write)
	{
		str = toString();
	}
	
	if (!_serializer_.value(str, _name))
	{
		return false;
	}

	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		fromString(str.c_str());
	}

	return true;
}

/*******************************************************************************

                               LocalReference

*******************************************************************************/

template <typename tReferent>
bool LocalReference<tReferent>::serialize(Serializer& _serializer_, const char* _name)
{
	SceneID::String str;

	if (_serializer_.getMode() == Serializer::Mode_Write)
	{
		str = id.toString();
	}

	if (_serializer_.beginArray(_name))
	{			
		_serializer_.value(str);

		if (_serializer_.getMode() == Serializer::Mode_Write)
		{
			String<24> name = referent ? referent->getName() : "--";
			_serializer_.value(name);
		}

		_serializer_.endArray();
	}
	else
	{
		_serializer_.setError("Error serializing LocalReference (%s).", _name ? _name : "--");
		return false;
	}
		
	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		id.fromString(str.c_str());
		referent = nullptr;
	}

	return true;
}
template struct LocalReference<SceneNode>;
template struct LocalReference<Component>;

/*******************************************************************************

                                GlobalReference

*******************************************************************************/

template <typename tReferent>
bool GlobalReference<tReferent>::serialize(Serializer& _serializer_, const char* _name)
{
	SceneID::String sceneStr;
	SceneID::String localStr;

	auto U16ToHexString = [](uint16 _u) -> SceneID::String
		{
			return (_u == 0) ? SceneID::String("0") : SceneID::String(SceneID::kStringFormat, _u);
		};

	auto HexStringToU16 = [](const char* _str) -> uint16
		{
			return (uint16)strtoul(_str, nullptr, 16);
		};


	if (_serializer_.getMode() == Serializer::Mode_Write)
	{
		sceneStr = U16ToHexString(id.scene);
		localStr = U16ToHexString(id.local);
	}

	if (_serializer_.beginArray(_name))
	{			
		_serializer_.value(sceneStr);
		_serializer_.value(localStr);

		if (_serializer_.getMode() == Serializer::Mode_Write)
		{
			String<24> name = referent ? referent->getName() : "--";
			_serializer_.value(name);
		}

		_serializer_.endArray();
	}
	else
	{
		_serializer_.setError("Error serializing GlobalReference (%s).", _name ? _name : "--");
		return false;
	}
		
	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		id.scene = HexStringToU16(sceneStr.c_str());
		id.local = HexStringToU16(localStr.c_str());
	}

	return true;
}
template struct GlobalReference<SceneNode>;
template struct GlobalReference<Component>;

/*******************************************************************************

                                 World

*******************************************************************************/

FRM_SERIALIZABLE_DEFINE(World, 0);

// PUBLIC

World* World::Create(const char* _path)
{
	World* world = FRM_NEW(World);
	
	if (_path && *_path)
	{
		Json json;
		if (!Json::Read(json, _path))
		{
			return world;
		}

		world->m_path = _path;

		SerializerJson serializer(json, SerializerJson::Mode_Read);
		world->serialize(serializer);
		if (serializer.getError())
		{		
			FRM_LOG_ERR("Error serializing world: %s", serializer.getError());
		}
	}
	
	return world;
}

void World::Destroy(World*& _world_)
{
	FRM_ASSERT(_world_->m_state == State::Shutdown);
	FRM_DELETE(_world_);
	_world_ = nullptr;
}

Camera* World::GetDrawCamera()
{
	return &GetDrawCameraComponent()->getCamera();
}

Camera* World::GetCullCamera()
{
	return &GetCullCameraComponent()->getCamera();
}

CameraComponent* World::GetDrawCameraComponent()
{
	World* world = GetCurrent();
	CameraComponent* cameraComponent = (CameraComponent*)world->m_drawCamera.referent;
	return cameraComponent ? cameraComponent : world->findOrCreateDefaultCamera();
}

CameraComponent* World::GetCullCameraComponent()
{
	World* world = GetCurrent();
	CameraComponent* cameraComponent = (CameraComponent*)world->m_cullCamera.referent;
	return cameraComponent ? cameraComponent : world->findOrCreateDefaultCamera();
}

void World::update(float _dt, UpdatePhase _phase)
{
	// \hack \todo Profiler markers don't support dynamic strings.
	static const char* kUpdatePhaseMarkerStr[(int)World::UpdatePhase::_Count] =
	{
		"#World::update(GatherActive)",
		"#World::update(PrePhysics)",
		"#World::update(Hierarchy)",
		"#World::update(Physics)",
		"#World::update(PostPhysics)",
		"#World::update(PreRender)"
	};

	if (_phase == UpdatePhase::All)
	{
		for (int i = 0; i < (int)UpdatePhase::_Count; ++i)
		{
			PROFILER_MARKER_CPU(kUpdatePhaseMarkerStr[i]);

			_phase = (UpdatePhase)i;

			if_likely (m_rootScene)
			{
				m_rootScene->update(_dt, _phase);
			}
			Component::Update(_dt, _phase);
		}
	}
	else
	{
		PROFILER_MARKER_CPU(kUpdatePhaseMarkerStr[(int)_phase]);

		if_likely (m_rootScene)
		{
			m_rootScene->update(_dt, _phase);
		}
		Component::Update(_dt, _phase);
	}
}

bool World::serialize(Serializer& _serializer_)
{
	Component::ClearActiveComponents(); // Active component ptrs are cached, need to clear these before we realloc.

	bool ret = SerializeAndValidateClass(_serializer_);
	if (!ret)
	{
		return false;
	}

	PathStr rootScenePath = m_rootScene ? m_rootScene->getPath() : "";
	
	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		if (Serialize(_serializer_, rootScenePath, "RootScenePath"))
		{
			Json rootJson;
			if (!Json::Read(rootJson, rootScenePath.c_str()))
			{
				_serializer_.setError("Failed to load root scene '%s'", rootScenePath.c_str());
				ret = false;
			}

			if (!m_rootScene)
			{
				m_rootScene = FRM_NEW(Scene(this));
			}
			else
			{
				removeSceneInstance(m_rootScene); // Need to remove before changing the path below.
			}
			m_rootScene->m_path = rootScenePath;
			
			SerializerJson rootSerializer(rootJson, _serializer_.getMode());
			if (!m_rootScene->serialize(rootSerializer))
			{
				_serializer_.setError(rootSerializer.getError());
				ret = false;
			}
		}
		else
		{
			if (_serializer_.beginObject("RootScene"))
			{
				if (!m_rootScene)
				{
					m_rootScene = FRM_NEW(Scene(this));
				}

				ret &= m_rootScene->serialize(_serializer_);
				_serializer_.endObject();
			}			
		}
	}
	else
	{
		if (m_rootScene && rootScenePath.isEmpty())
		{
			// Root scene has no path, serialize directly with the world.
			if (_serializer_.beginObject("RootScene"))
			{
				ret &= m_rootScene->serialize(_serializer_);
				_serializer_.endObject();
			}			
		}
		else
		{
			ret &= Serialize(_serializer_, rootScenePath, "RootScenePath");
		}
	}

	ret &= m_drawCamera.serialize(_serializer_, "Draw Camera");
	ret &= m_cullCamera.serialize(_serializer_, "Cull Camera");

	return ret;
}

bool World::init()
{
	FRM_ASSERT(m_state == State::Shutdown);
	m_state = World::State::Init;

	if (!m_rootScene)
	{
		m_rootScene = Scene::CreateDefault(this);
	}

	if (!m_rootScene->init())
	{
		return false;
	}

	// \hack \todo Explicit template instantiations are below this point in the file - split up the code.
	//m_rootScene->resolveReference(m_drawCamera[0]);
	//m_rootScene->resolveReference(m_cullCamera[0]);
	m_drawCamera.referent = m_rootScene->findComponent(m_drawCamera.id.local, m_drawCamera.id.scene);
	m_cullCamera.referent = m_rootScene->findComponent(m_cullCamera.id.local, m_cullCamera.id.scene);
	
	// Resolve the hierarchy once so that world transforms are set during postInit().
	update(0.f, UpdatePhase::Hierarchy);

	return true;
}

bool World::postInit()
{
	FRM_ASSERT(m_state == State::Init);
	m_state = World::State::PostInit;

	if (!m_rootScene->postInit())
	{
		return false;
	}

	return true;
}

void World::shutdown()
{
	FRM_ASSERT(m_state == State::PostInit);
	m_state = World::State::Shutdown;

	m_rootScene->shutdown();
	FRM_DELETE(m_rootScene);
	m_rootScene = nullptr;

	FRM_ASSERT(m_sceneInstances.empty());
}


// PRIVATE

World* World::s_current = nullptr;

World::World()
{
	if (s_current == nullptr)
	{
		s_current = this;
	}
}

World::~World()
{
	FRM_ASSERT(m_state == State::Shutdown);

	if (s_current == this)
	{
		s_current = nullptr;
	}
}

void World::addSceneInstance(Scene* _scene)
{
	const StringHash pathHash = StringHash(_scene->getPath().c_str());
	SceneList& instanceList = m_sceneInstances[pathHash];
	FRM_ASSERT_MSG(eastl::find(instanceList.begin(), instanceList.end(), _scene) == instanceList.end(), "Scene instance 0x%X ('%s') was already added to the world", _scene, _scene->getPath().c_str());
	instanceList.push_back(_scene);
}

void World::removeSceneInstance(Scene* _scene)
{
	const StringHash pathHash = StringHash(_scene->getPath().c_str());
	SceneList& instanceList = m_sceneInstances[pathHash];
	auto it = eastl::find(instanceList.begin(), instanceList.end(), _scene);
	//FRM_ASSERT_MSG(it != instanceList.end(), "Scene instance 0x%X ('%s') not found", _scene, _scene->getPath().c_str());
	if (it != instanceList.end()) // \todo \editoronly 
	{
		instanceList.erase_unsorted(it);
		if (instanceList.empty())
		{
			m_sceneInstances.erase(pathHash);
		}
	}
}

CameraComponent* World::findOrCreateDefaultCamera()
{
	CameraComponent* ret = nullptr;

	m_rootScene->traverse([&ret](SceneNode* _node) -> bool
		{
			if (!_node->getFlag(SceneNode::Flag::Active))
			{
				return false;
			}

			CameraComponent* cameraComponent = (CameraComponent*)_node->findComponent(StringHash("CameraComponent"));
			if (cameraComponent)
			{
				ret	= cameraComponent;
				return false; // End traversal.
			}

			return true;
		});

	if (!ret)
	{
		SceneNode* cameraNode = m_rootScene->createTransientNode("#DefaultCamera");

		ret = (CameraComponent*)Component::Create(StringHash("CameraComponent"));
		Camera& camera = ret->getCamera();
		camera.setPerspective(Radians(45.f), 16.f / 9.f, 0.1f, 1000.0f, Camera::ProjFlag_Infinite);
		
		FreeLookComponent* freeLookComponent = (FreeLookComponent*)Component::Create(StringHash("FreeLookComponent"));
		freeLookComponent->lookAt(vec3(0.f, 10.f, 64.f), vec3(0.f, 0.f, 0.f));
		
		cameraNode->addComponent(ret);
		cameraNode->addComponent(freeLookComponent);

		FRM_VERIFY(cameraNode->init() && cameraNode->postInit());

		SetDrawCameraComponent(ret);
		SetCullCameraComponent(ret);
	}

	return ret;
}

/*******************************************************************************

                                  Scene

*******************************************************************************/

FRM_SERIALIZABLE_DEFINE(Scene, 0);

// PUBLIC

bool Scene::serialize(Serializer& _serializer_)
{
	bool ret = SerializeAndValidateClass(_serializer_);
	if (!ret)
	{
		return false;
	}

	// \todo This editor-only shutdown code ends up being basically equivalent to calling shutdown() before re-serializing, the only benefit is potentially fewer allocations from the pool. Consider removing?
	// \todo \editoronly This is a re-serialization of an already initialized object, need to shutdown nodes so that the calls to init() and postInit() will work correctly.
	if (m_state != World::State::Shutdown && _serializer_.getMode() == Serializer::Mode_Read)
	{		
		for (auto& it : m_localNodeMap)
		{
			it.second->shutdown(); // Will also shutdown components.
		}
	}

	ret &= m_root.serialize(_serializer_, "Root");

	// Nodes.
	uint nodeCount = m_localNodeMap.size();
	if (_serializer_.beginArray(nodeCount, "Nodes"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			eastl::fixed_vector<SceneID, 128> validNodes; // Keep a list of nodes serialized during this invocation to reconcile the node map.
	
			for (uint i = 0; i < nodeCount; ++i)
			{
				if (_serializer_.beginObject())
				{
					SceneID localID;
					FRM_VERIFY(localID.serialize(_serializer_)); // \todo error

					SceneNode*& node = m_localNodeMap[localID];
					if (!node)
					{
						node = m_nodePool.alloc(SceneNode(this, localID));
					}
					node->m_parentScene = this;

					ret &= node->serialize(_serializer_);
					FRM_STRICT_ASSERT(node->getID() == localID);
					validNodes.push_back(localID);

					_serializer_.endObject();
				}
			}

			// Reconcile the node map (remove any nodes that weren't serialized above).
			for (auto it = m_localNodeMap.begin(); it != m_localNodeMap.end();)
			{
				auto found = eastl::find(validNodes.begin(), validNodes.end(), it->first);
				if (found == validNodes.end())
				{
					// \todo \editoronly This can happen in the editor when loading a world; need to purge global references recursively upwards.
					if (it->second->m_state != World::State::Shutdown)
					{
						it->second->shutdown();
					}
					m_nodePool.free(it->second);
					it = m_localNodeMap.erase(it);
				}
				else
				{
					++it; // Only increment if we don't call erase().
				}
			}
		}
		else
		{
			for (auto& it : m_localNodeMap)
			{
				SceneNode* node = it.second;				
				FRM_VERIFY(_serializer_.beginObject());
				ret &= node->serialize(_serializer_);
				_serializer_.endObject();
			}
		}
		
		_serializer_.endArray();
	}

	// Components.
	uint componentCount = m_localComponentMap.size();
	if (_serializer_.beginArray(componentCount, "Components"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			eastl::fixed_vector<SceneID, 128> validComponents; // Keep a list of components serialized during this invocation to reconcile the component map.

			for (uint i = 0; i < componentCount; ++i)
			{
				if (_serializer_.beginObject())
				{
					SceneID localID;
					FRM_VERIFY(localID.serialize(_serializer_)); // \todo error
					String<32> className;
					FRM_VERIFY(Serialize(_serializer_, className, "_class")); // \todo error
					StringHash classNameHash(className.c_str());

					Component*& component = m_localComponentMap[localID];
					if (component && component->getClassRef()->getNameHash() != classNameHash)
					{
						// \todo \editoronly This can happen in the editor when loading a world.
						if (component->m_state != World::State::Shutdown)
						{
							component->shutdown();
						}
						component->Destroy(component);
					}

					if (!component)
					{
						component = Component::Create(classNameHash);
					}

					if (component)
					{
						ret &= component->serialize(_serializer_);
						FRM_STRICT_ASSERT(component->getID() == localID);
						validComponents.push_back(localID);
					}
					else
					{
						FRM_LOG_ERR("World: Failed to create component '%s' - class does not exist", className.c_str());
						//ret = false;
					}

					_serializer_.endObject();
				}
			}

			// Reconcile the component map (remove any components that weren't serialized above).
			for (auto it = m_localComponentMap.begin(); it != m_localComponentMap.end();)
			{
				auto found = eastl::find(validComponents.begin(), validComponents.end(), it->first);
				if (found == validComponents.end())
				{
					Component* component = it->second;

					// \todo \editoronly This can happen in the editor when loading a world.
					if (component)
					{
						if (component->getState() != World::State::Shutdown)
						{
							component->shutdown();
						}
						Component::Destroy(component);
					}
					it = m_localComponentMap.erase(it);
				}
				else
				{
					++it; // Only increment if we don't call erase().
				}
			}
		}
		else
		{
			for (auto& it : m_localComponentMap)
			{
				Component* component = it.second;
				FRM_VERIFY(_serializer_.beginObject());
				ret &= component->serialize(_serializer_);
				_serializer_.endObject();
			}
		}
		
		_serializer_.endArray();
	}

	// \todo \editoronly This is a re-serialization of an already initialized object.
	if (m_state != World::State::Shutdown && _serializer_.getMode() == Serializer::Mode_Read)
	{
		m_state = World::State::Shutdown;
		m_world->removeSceneInstance(this);

		ret &= init();
		ret &= postInit();

		if (m_parentNode)
		{
			m_parentNode->m_parentScene->resetGlobalReferenceMap();
		}
	}

	return ret;
}

bool Scene::init()
{
	FRM_ASSERT(m_state == World::State::Shutdown);
	m_state = World::State::Init;

	bool ret = true;
	

	for (auto& it : m_localNodeMap)
	{
		SceneNode* node = it.second;
		/*ret &= */node->init(); // \todo Allow nodes to fail to initialize?
	}

	initGlobalReferenceMap();

	// Can't call resolveReference here as the explicit template instantiation is below.
	//FRM_VERIFY(resolveReference(m_root));
	m_root.referent = findNode(m_root.id);
	FRM_ASSERT(m_root.isResolved());

	m_world->addSceneInstance(this);

	return ret;
}

bool Scene::postInit()
{
	FRM_ASSERT(m_state == World::State::Init);
	m_state = World::State::PostInit;

	bool ret = true;

	for (auto& it : m_localNodeMap)
	{
		SceneNode* node = it.second;
		ret &= node->postInit();
	}

	return ret;
}

void Scene::shutdown()
{
	FRM_ASSERT(m_state == World::State::PostInit);
	m_state = World::State::Shutdown;

	destroyNode(m_root.referent); // Will cause all nodes to be recursively destroyed during flushPendingDeletes().
	flushPendingDeletes();
	m_localNodeMap.clear();
	m_globalNodeMap.clear();

	for (auto& it : m_localComponentMap)
	{
		Component::Destroy(it.second);
	}
	m_localComponentMap.clear();
	m_globalComponentMap.clear();

	if (m_parentNode)
	{
		m_parentNode->m_parentScene->resetGlobalReferenceMap();
	}

	m_world->removeSceneInstance(this);
}

void Scene::update(float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("Scene::update");

	if_unlikely (!m_root.isResolved())
	{
		return;
	}

	switch (_phase)
	{
		default:
			break;
		case World::UpdatePhase::GatherActive:
			flushPendingDeletes();
			break;
	};

	traverse([_dt, _phase](SceneNode* _node) -> bool
		{
			if (!_node->isActive())
			{
				return false;
			}					
			_node->update(_dt, _phase);
			return true;
		});
}

SceneNode* Scene::createNode(SceneID _id, const char* _name, SceneNode* _parent)
{
	FRM_ASSERT_MSG(m_localNodeMap.find(_id) == m_localNodeMap.end(), "Node ID [%s] already exists", _id.toString().c_str());

	SceneNode* ret = m_nodePool.alloc(SceneNode(this, _id, _name));
	m_localNodeMap[_id] = ret;
	
	if (_id > 1u) // Only set parent if *not* root.
	{
		ret->setParent(_parent);
	}

	if (m_parentNode)
	{
		// \todo could implement a more efficient solution which recursively adds a single node
		m_parentNode->m_parentScene->resetGlobalReferenceMap();
	}

	return ret;
}

SceneNode* Scene::createTransientNode(const char* _name, SceneNode* _parent)
{
	SceneNode* ret = m_nodePool.alloc(SceneNode(this, 0u, _name));
	ret->setParent(_parent);
	return ret;
}

void Scene::destroyNode(SceneNode* _node_)
{
	if (eastl::find(m_pendingDeletes.begin(), m_pendingDeletes.end(), _node_) == m_pendingDeletes.end())
	{
		m_pendingDeletes.push_back(_node_);
	}
}

void Scene::traverse(const eastl::function<bool(SceneNode*)>& _onVisit, SceneNode* _root)
{
	PROFILER_MARKER_CPU("Scene::traverse");

	_root = _root ? _root : m_root.referent;
	FRM_STRICT_ASSERT(_root);

	eastl::fixed_vector<SceneNode*, 32> tstack;
	tstack.push_back(_root);
	while (!tstack.empty())
	{
		SceneNode* node = tstack.back();
		tstack.pop_back();

		if (_onVisit(node))
		{
			for (LocalNodeReference& nodeReference : node->m_children)
			{
				FRM_STRICT_ASSERT(nodeReference.isResolved());
				tstack.push_back(nodeReference.referent);
			}

			if (node->m_childScene)
			{
				node->m_childScene->traverse(_onVisit);
			}
		}
	}
}

template <>
bool Scene::resolveReference(LocalNodeReference& _ref_)
{
	if (_ref_.id == 0u)
	{
		FRM_ASSERT_MSG(_ref_.isResolved(), "Unresolved local reference to transient node.");
		return true;
	}

	_ref_.referent = findNode(_ref_.id);
	if (!_ref_.isResolved())
	{
		return false;
	}

	return true;
}

template <>
bool Scene::resolveReference(LocalComponentReference& _ref_)
{
	if (_ref_.id == 0u)
	{
		FRM_ASSERT_MSG(_ref_.isResolved(), "Unresolved local reference to transient component.");
		return true;
	}

	_ref_.referent = findComponent(_ref_.id);
	if (!_ref_.isResolved())
	{
		return false;
	}

	return true;
}

template <>
bool Scene::resolveReference(GlobalNodeReference& _ref_)
{
	if (_ref_.id.local == 0u)
	{
		FRM_ASSERT_MSG(_ref_.isResolved(), "Unresolved global reference to transient node.");
		return true;
	}

	_ref_.referent = findNode(_ref_.id.local, _ref_.id.scene);
	if (!_ref_.referent)
	{
		return false;
	}

	FRM_ASSERT(_ref_.referent->getID() == _ref_.id.local);

	return true;
}

template <>
bool Scene::resolveReference(GlobalComponentReference& _ref_)
{
	if (_ref_.id.local == 0u)
	{
		FRM_ASSERT_MSG(_ref_.isResolved(), "Unresolved global reference to transient component.");
		return true;
	}

	_ref_.referent = findComponent(_ref_.id.local, _ref_.id.scene);
	if (!_ref_.referent)
	{
		return false;
	}

	FRM_ASSERT(_ref_.referent->getID() == _ref_.id.local);

	return true;
}

SceneNode* Scene::findNode(SceneID _localID, SceneID _sceneID) const
{
	if (_sceneID == 0u)
	{
		auto it = m_localNodeMap.find(_localID);
		if (it != m_localNodeMap.end())
		{
			return it->second;
		}
	}
	else
	{
		auto it = m_globalNodeMap.find(SceneGlobalID({ _sceneID, _localID }));
		if (it != m_globalNodeMap.end())
		{
			return it->second;
		}
	}
	
	return nullptr;
}

Component* Scene::findComponent(SceneID _localID, SceneID _sceneID) const
{
	if (_sceneID == 0u)
	{
		auto it = m_localComponentMap.find(_localID);
		if (it != m_localComponentMap.end())
		{
			return it->second;
		}
	}
	else
	{
		auto it = m_globalComponentMap.find(SceneGlobalID({ _sceneID, _localID }));
		if (it != m_globalComponentMap.end())
		{
			return it->second;
		}
	}

	return nullptr;
}

SceneID Scene::findUniqueNodeID() const
{
	SceneID ret;

	for (auto& it : m_localNodeMap)
	{
		ret = Max(ret.value, it.first.value);
	}
	++ret;

	return ret;
}

SceneID Scene::findUniqueComponentID() const
{
	SceneID ret;

	for (auto& it : m_localComponentMap)
	{
		ret = Max(ret.value, it.first.value);
	}
	++ret;

	return ret;
}

void Scene::setPath(const char* _path)
{
	if (m_path == _path)
	{
		return;
	}

	m_world->removeSceneInstance(this);
	m_path = _path;
	m_world->addSceneInstance(this);
}

template <>
GlobalNodeReference Scene::findGlobal(const SceneNode* _node) const
{
	if (_node->m_parentScene == this)
	{
		return GlobalNodeReference(0u, _node->m_id, const_cast<SceneNode*>(_node));
	}

	for (auto& it : m_globalNodeMap)
	{
		if (it.second == _node)
		{
			return GlobalNodeReference(it.first, it.second);
		}
	}

	return GlobalNodeReference();
}

template <>
GlobalComponentReference Scene::findGlobal(const Component* _component) const
{
	const SceneNode* node = _component->m_parentNode;

	if (node->m_parentScene == this)
	{
		return GlobalComponentReference(0u, _component->m_id, const_cast<Component*>(_component));
	}

	for (auto& it : m_globalComponentMap)
	{
		if (it.second == _component)
		{
			return GlobalComponentReference(it.first, it.second);
		}
	}

	return GlobalComponentReference();
}

// PRIVATE

Scene* Scene::CreateDefault(World* _world)
{
	Scene* scene = FRM_NEW(Scene(_world));
	return scene;
}

Scene::Scene(World* _world, SceneNode* _parentNode)
	: m_world(_world)
	, m_parentNode(_parentNode)
	, m_nodePool(128)
{
	m_root.id = 1u;
	m_root.referent = createNode(1u, "#Root");
}

Scene::~Scene()
{
	FRM_ASSERT(m_state == World::State::Shutdown);
}

void Scene::addComponent(Component* _component)
{
	SceneID id = _component->getID();
	FRM_ASSERT_MSG(m_localComponentMap.find(id) == m_localComponentMap.end(), "Component [%s] (%s) already exists", id.toString().c_str(), _component->getName());
	m_localComponentMap[id] = _component;

	if (m_parentNode)
	{
		m_parentNode->m_parentScene->resetGlobalReferenceMap();
	}
}

void Scene::removeComponent(Component* _component)
{
	SceneID id = _component->getID();
	FRM_ASSERT(id != 0u);
	
	auto it = m_localComponentMap.find(id);
	FRM_ASSERT(it != m_localComponentMap.end()); // not found
	m_localComponentMap.erase(it);

	if (m_parentNode)
	{
		m_parentNode->m_parentScene->resetGlobalReferenceMap();
	}
}

void Scene::initGlobalReferenceMap()
{
	PROFILER_MARKER_CPU("Scene::initGlobalReferenceMap");

	m_globalNodeMap.clear();
	m_globalComponentMap.clear();

	// For each local node with a child scene.
	for (auto& childIt : m_localNodeMap)
	{
		if (!childIt.second->m_childScene)
		{
			continue;
		}

		const SceneID sceneID = childIt.first;
		
		// Append child scene's local nodes/components.
		for (auto sceneIt : childIt.second->m_childScene->m_localNodeMap)
		{
			SceneGlobalID globalID = { sceneID, sceneIt.first };
			m_globalNodeMap[globalID] = sceneIt.second;
		}
		for (auto sceneIt : childIt.second->m_childScene->m_localComponentMap)
		{		
			SceneGlobalID globalID = { sceneID, sceneIt.first };
			m_globalComponentMap[globalID] = sceneIt.second;
		}

		// Append child scene's global node/component maps.
		for (auto sceneIt : childIt.second->m_childScene->m_globalNodeMap)
		{
			const SceneID hashedSceneID = SceneID(sceneIt.first.scene, sceneID);
			SceneGlobalID globalID = { hashedSceneID, sceneIt.first.local };
			m_globalNodeMap[globalID] = sceneIt.second;
		}
		for (auto sceneIt : childIt.second->m_childScene->m_globalComponentMap)
		{
			const SceneID hashedSceneID = SceneID(sceneIt.first.scene, sceneID);
			SceneGlobalID globalID = { hashedSceneID, sceneIt.first.local };
			m_globalComponentMap[globalID] = sceneIt.second;
		}
	}
}

void Scene::resetGlobalReferenceMap()
{
	PROFILER_MARKER_CPU("Scene::resetGlobalReferenceMap");

	initGlobalReferenceMap();

	if (m_parentNode)
	{
		m_parentNode->m_parentScene->resetGlobalReferenceMap();
	}
}

void Scene::flushPendingDeletes()
{
	PROFILER_MARKER_CPU("Scene::flushPendingDeletes");

	bool requireGlobalReferenceMapReset = false;	
	while (!m_pendingDeletes.empty())
	{
		// Calling shutdown() on a node below may append to m_pendingDeletes, hence process the list iteratively.
		eastl::vector<SceneNode*> pendingDeletes;
		eastl::swap(m_pendingDeletes, pendingDeletes);

		while (!pendingDeletes.empty())
		{
			SceneNode* node = pendingDeletes.back();
			pendingDeletes.pop_back();
		
			FRM_ASSERT(node);
			FRM_ASSERT(node->m_parentScene == this);

			if (node->getState() != World::State::Shutdown)
			{
				node->shutdown();
			}

			// \todo Handle this outside the call to flushPendingDeletes() to manage different behaviors e.g. reparent.
			for (LocalNodeReference& child : node->m_children)
			{
				child->m_parent = LocalNodeReference();
				destroyNode(child.referent);
			}

			if (node->isTransient())
			{
				// Transient nodes can simply be deleted.
				FRM_ASSERT(node->getID() == 0u);
				m_nodePool.free(node);	
			}
			else
			{
				// Removal from the parent is automatic for transient nodes (happens during shutdown). Permanent nodes must do this manually.
				if (node->m_parent.isResolved())
				{
					node->m_parent->removeChild(node);
				}

				// Permanent nodes must be removed from the local/global node maps.
				SceneID id = node->getID();
				FRM_ASSERT(id != 0u);
				auto it = m_localNodeMap.find(id);
				FRM_ASSERT(it != m_localNodeMap.end());
				m_localNodeMap.erase(it);
				m_nodePool.free(node);
				requireGlobalReferenceMapReset = true;
			}
		}
	}
	
	if (requireGlobalReferenceMapReset)
	{
		resetGlobalReferenceMap();
	}
}


/*******************************************************************************

                                 SceneNode

*******************************************************************************/

FRM_SERIALIZABLE_DEFINE(SceneNode, 0);

// PUBLIC

void SceneNode::update(float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("SceneNode::update");

	switch (_phase)
	{
		default:
			break;
		case World::UpdatePhase::GatherActive:
		{
			m_local = m_initial;

			FRM_ASSERT(m_flags.get(Flag::Active)); // Should skip inactive nodes during scene traversal.
			for (LocalComponentReference& component : m_components)
			{
				component->setActive();
			}
			break;
		}
		case World::UpdatePhase::Hierarchy:
		{
			if (m_parent.isResolved())
			{
				m_world = m_parent->m_world * m_local;
			}
			else if (m_parentScene->m_parentNode)
			{
				m_world = m_parentScene->m_parentNode->m_world * m_local;
			}
			else
			{
				m_world = m_local;
			}

			break;
		}
	};
}

bool SceneNode::serialize(Serializer& _serializer_)
{
	bool ret = SerializeAndValidateClass(_serializer_);
	if (!ret)
	{
		return false;
	}

	static const char* kFlagNames[] = { "Active", "Static", "Transient" };
	ret &= m_id.serialize(_serializer_);
	ret &= Serialize(_serializer_, m_name, "Name");
	ret &= Serialize(_serializer_, m_flags, kFlagNames, "Flags");
	ret &= Serialize(_serializer_, m_initial, "Transform");

	if (_serializer_.beginObject("Hierarchy"))
	{
		ret &= m_parent.serialize(_serializer_, "Parent");

		uint childCount = m_children.size();
		if (_serializer_.beginArray(childCount, "Children"))
		{
			if (_serializer_.getMode() == Serializer::Mode_Read)
			{
				m_children.resize(childCount);
			}

			for (uint i = 0; i < childCount; ++i)
			{
				if (_serializer_.getMode() == Serializer::Mode_Write && m_children[i]->isTransient())
				{
					continue;
				}

				m_children[i].serialize(_serializer_);
			}

			_serializer_.endArray();
		}
		m_children.shrink_to_fit();

		_serializer_.endObject();
	}

	uint componentCount = m_components.size();
	if (_serializer_.beginArray(componentCount, "Components"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			m_components.resize(componentCount);
		}

		for (uint i = 0; i < componentCount; ++i)
		{
			if (_serializer_.getMode() == Serializer::Mode_Write && m_components[i]->getID() == 0u)
			{
				continue;
			}

			m_components[i].serialize(_serializer_);
		}
		m_components.shrink_to_fit();

		_serializer_.endArray();
	}

	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		PathStr childScenePath;
		if (Serialize(_serializer_, childScenePath, "ChildScene"))
		{
			Json json;
			if (Json::Read(json, childScenePath.c_str()))
			{
				if (!m_childScene)
				{
					m_childScene = FRM_NEW(Scene(m_parentScene->m_world, this)); // \todo create/destroy methods on Scene?
				}
				m_childScene->m_path = childScenePath;

				SerializerJson childSceneSerializer(json, _serializer_.getMode());
				if (!m_childScene->serialize(childSceneSerializer))
				{
					_serializer_.setError(childSceneSerializer.getError());
					ret = false;
				}
			}
		}
	}
	else if (_serializer_.getMode() == Serializer::Mode_Write && m_childScene)
	{
		PathStr childScenePath = m_childScene->getPath();
		ret &= Serialize(_serializer_, childScenePath, "ChildScene");
	}

	return ret;
}

bool SceneNode::init()
{
	FRM_ASSERT(m_state == World::State::Shutdown);
	m_state = World::State::Init;

	bool ret = true;

	if (m_childScene)
	{
		ret &= m_childScene->init();
	}

	// Resolve hierarchy references.
	if (m_parent.id != 0u)
	{
		FRM_VERIFY(m_parentScene->resolveReference(m_parent));
	}
	for (LocalNodeReference& child : m_children)
	{
		FRM_ASSERT(child.id != 0u); // \todo valid to have a transient child at this point?
		FRM_VERIFY(m_parentScene->resolveReference(child));
	}

	// Resolve component references, init components and remove invalid references.
	bool staticState = true;
	for (auto it = m_components.begin(); it != m_components.end();)
	{
		LocalComponentReference& component = *it;

		if (m_parentScene->resolveReference(component))
		{
			component->setParentNode(this);
			staticState &= component->isStatic();
			ret &= component->init();
			++it;
		}
		else
		{
			it = m_components.erase(it);
		}
	}
	m_flags.set(Flag::Static, staticState);

	dispatchCallbacks(Event::OnInit);
	
	// \todo if init fails, put the component into an error state? 
	FRM_ASSERT(ret);

	return ret;
}

bool SceneNode::postInit()
{
	FRM_ASSERT(m_state == World::State::Init);
	m_state = World::State::PostInit;

	bool ret = true;
	
	if (m_childScene)
	{
		ret &= m_childScene->postInit();
	}

	for (LocalComponentReference& component : m_components)
	{
		ret &= component->postInit();
	}

	dispatchCallbacks(Event::OnPostInit);
	
	// \todo if postInit fails, put the component into an error state? 
	FRM_ASSERT(ret);

	return ret;
}

void SceneNode::shutdown()
{
	FRM_ASSERT(m_state == World::State::PostInit);
	m_state = World::State::Shutdown;

	dispatchCallbacks(Event::OnShutdown);

	// At this point, any transient children should be destroyed.
	for (LocalNodeReference& child : m_children)
	{
		if (child->isTransient())
		{
			FRM_ASSERT(child->getID() == 0u);
			m_parentScene->destroyNode(child.referent);
		}
		child->m_parent.referent = nullptr;
	}

	if (m_childScene)
	{
		m_childScene->shutdown();
		FRM_DELETE(m_childScene); // \todo Create/Destroy members on Scene?
		m_childScene = nullptr;
	}

	for (LocalComponentReference& component : m_components)
	{
		component->shutdown();
		if (component->getID() == 0u)
		{
			// Destroy transient components.
			// \todo Remove reference from the component list.
			Component::Destroy(component.referent);
		}
	}

	for (auto& callbackList : m_callbacks)
	{
		callbackList.clear();
	}

	if (m_parent.isResolved() && isTransient())
	{
		m_parent->removeChild(this);
	}
}

void SceneNode::addComponent(Component* _component)
{
	FRM_ASSERT(_component);
	FRM_ASSERT(_component->getParentNode() == nullptr);

	_component->setParentNode(this);
	m_components.push_back(LocalComponentReference(_component));
	updateStaticState();

	if (_component->getID() != 0u)
	{
		m_parentScene->addComponent(_component); // add non-transient components to scene
	}

	// If the node is init, need to init the component.
	if (m_state == World::State::PostInit && _component->getState() != World::State::PostInit)
	{
		_component->init();
		_component->postInit();
	}
}

void SceneNode::removeComponent(Component* _component)
{
	auto it = eastl::find(m_components.begin(), m_components.end(), LocalComponentReference(_component));
	FRM_ASSERT(it != m_components.end());
	FRM_ASSERT(_component->getParentNode() == this);
	m_components.erase(it);
	updateStaticState();
	if (it->id != 0u)
	{
		m_parentScene->removeComponent(_component); // remove non-transient components from scene
	}
	_component->shutdown();
	Component::Destroy(_component);
}

Component* SceneNode::findComponent(StringHash _className)
{
	FRM_ASSERT(m_state != World::State::Shutdown);

	for (LocalComponentReference& component : m_components)
	{
		if (component->getClassRef()->getNameHash() == _className)
		{
			return component.referent;
		}
	}

	return nullptr;
}

void SceneNode::registerCallback(SceneNode::Event _event, Callback* _callback, void* _arg)
{
	CallbackList& callbackList = m_callbacks[(int)_event];
	CallbackListEntry callback = { _callback, _arg };
	FRM_ASSERT(eastl::find(callbackList.begin(), callbackList.end(), callback) == callbackList.end()); // double registration
	callbackList.push_back(callback);
}

void SceneNode::unregisterCallback(SceneNode::Event _event, Callback* _callback, void* _arg)
{
	CallbackList& callbackList = m_callbacks[(int)_event];
	CallbackListEntry callback = { _callback, _arg };
	auto it = eastl::find(callbackList.begin(), callbackList.end(), callback);
	FRM_ASSERT(it != callbackList.end()); // not found
	callbackList.erase_unsorted(it);
}


void SceneNode::setParent(SceneNode* _parent_)
{
	if (_parent_ && _parent_ == m_parent.referent)
	{
		return;
	}

	FRM_ASSERT_MSG(_parent_ != this, "Node cannot be a parent to itself");
	if (_parent_ == this)
	{
		return;
	}

	if (!_parent_)
	{
		// Only the scene root may have a null parent, otherwise the node would be unreachable. Force scene root parent in this case.
		setParent(m_parentScene->getRootNode());
		return;
	}

	//FRM_ASSERT(_parent_->getState() == World::State::PostInit); // \todo necessary for the parent to be init?
	FRM_ASSERT_MSG(_parent_->m_parentScene == m_parentScene, "Parent node must be in the same scene as children");
	FRM_ASSERT_MSG(_parent_->isTransient() == isTransient() || !_parent_->isTransient(), "Non-transient nodes may not have a transient parent");

	if (m_parent.isResolved())
	{
		// Preserve world space position when changing parent.
		mat4 parentWorld = m_parent->m_world;
		mat4 childWorld = parentWorld * m_local;
		m_local = inverse(_parent_->m_world) * childWorld;

		m_parent->m_children.erase_unsorted(m_parent->findChild(this));
	}

	_parent_->m_children.push_back(LocalNodeReference(this));
	m_parent = LocalNodeReference(_parent_);
}

void SceneNode::addChild(SceneNode* _child_)
{
	_child_->setParent(this);
}

void SceneNode::setChildScene(Scene* _scene_)
{
	if (m_childScene)
	{
		if (m_childScene->m_state != World::State::Shutdown)
		{
			m_childScene->shutdown();
		}

		FRM_DELETE(m_childScene);
	}

	m_childScene = _scene_;
	if (m_state == World::State::PostInit && m_childScene->m_state != World::State::PostInit)
	{
		m_childScene->init();
		m_childScene->postInit();
	}
	m_childScene->m_parentNode = this;
	m_parentScene->resetGlobalReferenceMap();
}

void SceneNode::setFlag(Flag _flag, bool _value)
{	
	m_flags.set(_flag, _value);
	// \todo dispatch callbacks?
}


// PRIVATE

SceneNode::SceneNode(Scene* _parentScene, SceneID _id, const char* _name)
	: m_parentScene(_parentScene)
	, m_id(_id)
{
 // NB Can't do any work in the ctor which makes use of the object address, because we construct and then move into the pool allocation.

	FRM_ASSERT(_parentScene);

	if (_name)
	{
		m_name = _name;
	}
	else
	{
		m_name = String<24>("Node_%s", _id.toString().c_str());
	}

	// Automatically set transient flag.
	setFlag(Flag::Transient, _id == 0u);
}

SceneNode::~SceneNode()
{
	FRM_ASSERT_MSG(getState() == World::State::Shutdown, "Node '%s' [%s] was not shutdown before being destroyed", getName(), getID().toString().c_str());
}

void SceneNode::dispatchCallbacks(Event _event)
{
	CallbackList& callbackList = m_callbacks[(int)_event];
	for (CallbackListEntry& callback : callbackList)
	{
		callback(this);
	}
}

void SceneNode::updateStaticState()
{
	bool staticState = true;
	for (LocalComponentReference& component : m_components)
	{
		staticState &= component->isStatic();
	}
	setFlag(Flag::Static, staticState);
}

void SceneNode::removeChild(SceneNode* _child_)
{
	FRM_ASSERT(_child_->m_parent == this);
	m_children.erase_unsorted(findChild(_child_));
	_child_->m_parent = LocalNodeReference();
}

SceneNode::ChildList::iterator SceneNode::findChild(const SceneNode* _child)
{
	return eastl::find(m_children.begin(), m_children.end(), LocalNodeReference((SceneNode*)_child));
}


// \todo These definitions are here because they need to call templated findGlobal(), need to split code up.
void World::SetDrawCameraComponent(CameraComponent* _cameraComponent)
{
	World* world = GetCurrent();
	Scene* rootScene = world->getRootScene();
	GlobalComponentReference ref = rootScene->findGlobal((Component*)_cameraComponent);
	if (ref.isValid() && ref.isResolved())
	{
		world->m_drawCamera = ref;
	}
	else
	{
		FRM_LOG_ERR("World::SetDrawCamera: %s camera component reference.", ref.isValid() ? "Unresolved" : "Invalid");
	}
}

void World::SetCullCameraComponent(CameraComponent* _cameraComponent)
{
	World* world = GetCurrent();
	Scene* rootScene = world->getRootScene();
	GlobalComponentReference ref = rootScene->findGlobal((Component*)_cameraComponent);
	if (ref.isValid() && ref.isResolved())
	{
		world->m_cullCamera = ref;
	}
	else
	{
		FRM_LOG_ERR("World::SetCullCamera: %s camera component reference.", ref.isValid() ? "Unresolved" : "Invalid");
	}
}

} // namespace frm