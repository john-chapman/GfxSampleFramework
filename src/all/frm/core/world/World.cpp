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
	CameraComponent* drawCameraComponent = CameraComponent::GetDrawCamera();
	return drawCameraComponent ? &drawCameraComponent->getCamera() : nullptr;
}

Camera* World::GetCullCamera()
{
	CameraComponent* cullCameraComponent = CameraComponent::GetCullCamera();
	return cullCameraComponent ? &cullCameraComponent->getCamera() : nullptr;
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

	if (!ret)
	{
		return false;
	}
	
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

	return m_rootScene->init();
}

bool World::postInit()
{
	FRM_ASSERT(m_state == State::Init);
	m_state = World::State::PostInit;
	return m_rootScene->postInit();
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

void World::update(float _dt, UpdatePhase _phase)
{

	if (_phase == UpdatePhase::All)
	{
		for (int i = 0; i < (int)UpdatePhase::_Count; ++i)
		{
			_phase = (UpdatePhase)i;

			// PROFILER_MARKER_CPU("Phase##"); // \todo
			if_likely (m_rootScene)
			{
				m_rootScene->update(_dt, _phase);
			}
			Component::Update(_dt, _phase);
		}
	}
	else
	{
		// PROFILER_MARKER_CPU("Phase##"); // \todo
		if_likely (m_rootScene)
		{
			m_rootScene->update(_dt, _phase);
		}
		Component::Update(_dt, _phase);
	}
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
	FRM_ASSERT_MSG(it != instanceList.end(), "Scene instance 0x%X ('%s') not found", _scene, _scene->getPath().c_str());
	instanceList.erase_unsorted(it);
	if (instanceList.empty())
	{
		m_sceneInstances.erase(pathHash);
	}
}


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

/*******************************************************************************

                               GlobalReference

*******************************************************************************/

GlobalNodeReference::GlobalNodeReference(SceneGlobalID _id, SceneNode* _node)
	: id(_id)
	, node(_node)
{
}

GlobalNodeReference::GlobalNodeReference(SceneID _sceneID, SceneID _localID, SceneNode* _node)
{
	id = { _sceneID, _localID };
	node = _node;
}

bool GlobalNodeReference::serialize(Serializer& _serializer_, const char* _name)
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
			String<24> name = node ? node->getName() : "--";
			_serializer_.value(name);
		}

		_serializer_.endArray();
	}
	else
	{
		_serializer_.setError("Error serializing GlobalNodeReference (%s).", _name ? _name : "--");
		return false;
	}
		
	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		id.scene = HexStringToU16(sceneStr.c_str());
		id.local = HexStringToU16(localStr.c_str());
	}

	return true;
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
	uint componentCount = m_componentMap.size();
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

					Component*& component = m_componentMap[localID];
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
			for (auto it = m_componentMap.begin(); it != m_componentMap.end();)
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
					it = m_componentMap.erase(it);
				}
				else
				{
					++it; // Only increment if we don't call erase().
				}
			}
		}
		else
		{
			for (auto& it : m_componentMap)
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
			m_parentNode->m_parentScene->resetGlobalNodeMap();
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
		ret &= node->init();
	}

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

	for (auto& it : m_localNodeMap)
	{
		it.second->shutdown();
	}
	for (auto& it : m_localNodeMap) // Need to free nodes in a second loop, because a node's memory may be read by its parent during shutdown().
	{
		m_nodePool.free(it.second);
	}
	m_localNodeMap.clear();
	m_globalNodeMap.clear();

	for (auto& it : m_componentMap)
	{
		Component::Destroy(it.second);
	}
	m_componentMap.clear();

	if (m_parentNode)
	{
		m_parentNode->m_parentScene->resetGlobalNodeMap();
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

	traverse([_dt, _phase](SceneNode* _node) -> bool
		{
			if (!_node->getFlag(SceneNode::Flag::Active))
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
		m_parentNode->m_parentScene->resetGlobalNodeMap();
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
	FRM_ASSERT(_node_);
	if (_node_->getState() != World::State::Shutdown)
	{
		_node_->shutdown();
	}

	if (_node_->getFlag(SceneNode::Flag::Transient))
	{
		// Transient nodes can simply be deleted.
		FRM_ASSERT(_node_->getID() == 0u);
		m_nodePool.free(_node_);	
	}
	else
	{
		// Removal from the parent is automatic for transient nodes (happens during shutdown). Permanent nodes must do this manually.
		if (_node_->m_parent.isResolved())
		{
			_node_->m_parent->removeChild(_node_);
		}

		// Permanent nodes must be removed from the local/global node maps.
		SceneID id = _node_->getID();
		FRM_ASSERT(id != 0u);
		auto it = m_localNodeMap.find(id);
		FRM_ASSERT(it != m_localNodeMap.end());
		m_localNodeMap.erase(it);
		m_nodePool.free(_node_);
		resetGlobalNodeMap();
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
			for (LocalReference<SceneNode>& nodeReference : node->m_children)
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
bool Scene::resolveReference(LocalReference<SceneNode>& _ref_)
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
bool Scene::resolveReference(LocalReference<Component>& _ref_)
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

	_ref_.node = findNode(_ref_.id.local, _ref_.id.scene);
	if (!_ref_.node)
	{
		return false;
	}

	FRM_ASSERT(_ref_.node->getID() == _ref_.id.local);

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

		return nullptr;
	}
	else
	{
		auto it = m_globalNodeMap.find(SceneGlobalID({ _sceneID, _localID }));
		if (it != m_globalNodeMap.end())
		{
			return it->second;
		}

		return nullptr;
	}
}

Component* Scene::findComponent(SceneID _localID) const
{
	auto it = m_componentMap.find(_localID);
	if (it != m_componentMap.end())
	{
		return it->second;
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

	for (auto& it : m_componentMap)
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

// PRIVATE

Scene* Scene::CreateDefault(World* _world)
{
	Scene* scene = FRM_NEW(Scene(_world));

	SceneNode* cameraNode = scene->createNode(2u, "#Camera1");

	CameraComponent* cameraComponent = (CameraComponent*)Component::Create(StringHash("CameraComponent"), 1u);
	Camera& camera = cameraComponent->getCamera();
	camera.setPerspective(Radians(45.f), 16.f / 9.f, 0.1f, 1000.0f, Camera::ProjFlag_Infinite);
	
	FreeLookComponent* freeLookComponent = (FreeLookComponent*)Component::Create(StringHash("FreeLookComponent"), 2u);
	freeLookComponent->lookAt(vec3(0.f, 10.f, 64.f), vec3(0.f, 0.f, 0.f));
	
	cameraNode->addComponent(cameraComponent);
	cameraNode->addComponent(freeLookComponent);

	return scene;
}

Scene::Scene(World* _world)
	: m_world(_world)
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
	FRM_ASSERT_MSG(m_componentMap.find(id) == m_componentMap.end(), "Component [%s] (%s) already exists", id.toString().c_str(), _component->getName());
	m_componentMap[id] = _component;
}

void Scene::removeComponent(Component* _component)
{
	SceneID id = _component->getID();
	FRM_ASSERT(id != 0u);
	
	auto it = m_componentMap.find(id);
	FRM_ASSERT(it != m_componentMap.end()); // not found
	m_componentMap.erase(it);
}

void Scene::resetGlobalNodeMap()
{
	m_globalNodeMap.clear();

	for (auto& childIt : m_localNodeMap)
	{
		if (!childIt.second->m_childScene)
		{
			continue;
		}
		
		for (auto sceneIt : childIt.second->m_childScene->m_localNodeMap)
		{
			SceneGlobalID globalID = { childIt.first, sceneIt.first };
			m_globalNodeMap[globalID] = sceneIt.second;
		}

		for (auto sceneIt : childIt.second->m_childScene->m_globalNodeMap)
		{
			SceneGlobalID globalID = { SceneID(sceneIt.first.scene, childIt.first), sceneIt.first.local };
			m_globalNodeMap[globalID] = sceneIt.second;
		}
	}

	if (m_parentNode)
	{
		m_parentNode->m_parentScene->resetGlobalNodeMap();
	}
}

GlobalNodeReference Scene::findGlobal(const SceneNode* _node) const
{
	for (auto& it : m_globalNodeMap)
	{
		if (it.second == _node)
		{
			return GlobalNodeReference(it.first, it.second);
		}
	}

	return GlobalNodeReference();
}

LocalNodeReference Scene::findLocal(const SceneNode* _node) const
{
	for (auto& it : m_localNodeMap)
	{
		if (it.second == _node)
		{
			return LocalNodeReference(it.first, it.second);
		}
	}

	return LocalNodeReference();
}


/*******************************************************************************

                                 SceneNode

*******************************************************************************/

FRM_SERIALIZABLE_DEFINE(SceneNode, 0);

// PUBLIC

bool SceneNode::serialize(Serializer& _serializer_)
{
	bool ret = SerializeAndValidateClass(_serializer_);
	if (!ret)
	{
		return false;
	}

	static const char* kFlagNames[] = { "Active", "Transient" };
	ret &= m_id.serialize(_serializer_);
	ret &= Serialize(_serializer_, m_name, "Name");
	ret &= Serialize(_serializer_, m_flags, kFlagNames, "Flags");
	ret &= Serialize(_serializer_, m_local, "Local");

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
				if (_serializer_.getMode() == Serializer::Mode_Write && m_children[i]->getFlag(SceneNode::Flag::Transient))
				{
					continue;
				}

				m_children[i].serialize(_serializer_);
			}

			_serializer_.endArray();
		}		

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

		_serializer_.endArray();
	}

	// \todo child scene

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
	for (auto it = m_components.begin(); it != m_components.end();)
	{
		LocalComponentReference& component = *it;

		if (m_parentScene->resolveReference(component))
		{
			component->setParentNode(this);
			ret &= component->init();
			++it;
		}
		else
		{
			it = m_components.erase(it);
		}
	}

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
		if (child->getFlag(Flag::Transient))
		{
			FRM_ASSERT(child->getID() == 0u);
			m_parentScene->destroyNode(child.referent);
		}
	}

	if (m_childScene)
	{
		m_childScene->shutdown();
		FRM_DELETE(m_childScene); FRM_ASSERT(false); // \todo Create/Destroy members on Scene?
	}

	m_children.clear();

	for (LocalComponentReference& component : m_components)
	{
		component->shutdown();
		if (component->getID() == 0u)
		{
			// Destroy transient components.
			Component::Destroy(component.referent);
		}
	}
	m_components.clear();

	for (auto& callbackList : m_callbacks)
	{
		callbackList.clear();
	}

	if (getFlag(Flag::Transient))
	{
		m_parent->removeChild(this);
	}
}

void SceneNode::update(float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("SceneNode::update");

	switch (_phase)
	{
		default:
			break;
		case World::UpdatePhase::GatherActive:
		{
			FRM_ASSERT(m_flags.get(Flag::Active)); // Should skip inactive nodes during scene traversal.
			for (LocalComponentReference& component : m_components)
			{
				component->setActive(true);
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

void SceneNode::addComponent(Component* _component)
{
	FRM_ASSERT(_component);
	FRM_ASSERT(_component->getParentNode() == nullptr);

	_component->setParentNode(this);
	m_components.push_back(LocalReference<Component>(_component));

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
	auto it = eastl::find(m_components.begin(), m_components.end(), LocalReference<Component>(_component));
	FRM_ASSERT(it != m_components.end());
	FRM_ASSERT(_component->getParentNode() == this);
	m_components.erase(it);
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
	FRM_ASSERT_MSG(_parent_->getFlag(Flag::Transient) == getFlag(Flag::Transient) || !_parent_->getFlag(Flag::Transient), "Non-transient nodes may not have a transient parent");

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

void SceneNode::setFlag(Flag _flag, bool _value)
{	
	m_flags.set(_flag, _value);
	// \todo dispatch callbacks?
}

void SceneNode::removeChild(SceneNode* _child_)
{
	FRM_ASSERT(_child_->m_parent.referent == this);
	m_children.erase_unsorted(findChild(_child_));
	_child_->m_parent = LocalNodeReference();
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

SceneNode::ChildList::iterator SceneNode::findChild(const SceneNode* _child)
{
	return eastl::find(m_children.begin(), m_children.end(), LocalNodeReference((SceneNode*)_child));
}

} // namespace frm