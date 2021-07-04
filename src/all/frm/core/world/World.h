/*	\todo
	- Bug whereby components of deleted nodes are still serialized out? May be an artefact of a previous bug.
	- Split up the code to make it more manageable and to fix some declaration order issues with explicit template instantiations.
	- Allocate scenes from a pool?
	- See \todo \editoronly in the code some operations require special handling in editor (like re-serialization, which doesn't happen at runtim).
	- Lifetime issues:
		- World objects and components have the following flow:
			- Allocate = reserve memory for the object.
			- Serialize (construct) = load the object metadata ready for init. Object is now safely referencable (it has a name and ID).
			- Init = load resources, register with other systems etc.
			- PostInit = anything further init that might require other objects to be init.
			- Shutdown = unload resources, return to a valid but shutdown state which can be init again.
		- Use case is to be able to shutdown() and then subsequently init()/postInit() on the world to restore everything.
			- This means that component shutdown() can't permanently delete resources, the dtor needs to do it instead (see XFormComponent).
			- Also, this means that any resources which get loaded during init()/postInit() will be re-loaded.
		- Solutions:
			- Better way to enforce resource usage through the API?
			- Load some resources during the serialize phase, free in the dtor. These will stay resident until the dtor is called.
*/
#pragma once

#include <frm/core/frm.h>
#include <frm/core/types.h>
#include <frm/core/math.h>
#include <frm/core/BitFlags.h>
#include <frm/core/Pool.h>
#include <frm/core/Serializable.h>
#include <frm/core/String.h>
#include <frm/core/StringHash.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/map.h>

namespace frm { 

////////////////////////////////////////////////////////////////////////////////
// SceneID
// Basic scene ID, unique for objects (nodes, components) within a scene.
////////////////////////////////////////////////////////////////////////////////
struct SceneID
{
	using String = String<4>;
	static constexpr char* kStringFormat = "%04X";

	uint16 value;

	       SceneID(uint16 _value = 0u)                                    { value = _value; }
	       SceneID(uint16 _base, uint16 _value);
	       ~SceneID()                                                     = default;

           operator uint16&()                                             { return value; }
	       operator const uint16&() const                                 { return value; }

	String toString() const                                               { return String(kStringFormat, value); }
	void   fromString(const char* _str);
	bool   serialize(Serializer& _serializer_, const char* _name = "ID");
};

////////////////////////////////////////////////////////////////////////////////
// SceneGlobalID
// Composite ID used to reference an object within a scene subtree. 
//
// A scene stores references to *all* nodes in the subtree below it; each scene 
// will therefore have a different global ID for a particular object further down
// the hierarchy.
////////////////////////////////////////////////////////////////////////////////
struct SceneGlobalID
{
	SceneID scene = 0u; // Hash of child scene parent node IDs down the hierarchy.
	SceneID local = 0u; // Object ID relative to its parent scene.

	bool    operator==(const SceneGlobalID& _rhs) const { return getPacked() == _rhs.getPacked(); }
	bool    operator!=(const SceneGlobalID& _rhs) const { return !(*this == _rhs); }
	bool    operator< (const SceneGlobalID& _rhs) const { return getPacked() <  _rhs.getPacked(); }

private:

	uint32  getPacked() const { return ((uint32)scene << 16) | (uint32)local; }
};

////////////////////////////////////////////////////////////////////////////////
// LocalReference
// Reference an object within a single scene.
////////////////////////////////////////////////////////////////////////////////
template <typename tReferent>
struct LocalReference
{
	SceneID          id       = 0u;
	tReferent*       referent = nullptr;

	                 LocalReference()                                  = default;
	                 LocalReference(SceneID _id, tReferent* _referent) { FRM_ASSERT(!referent || referent->getID() == _id); referent = _referent; id = _id; }
	                 LocalReference(tReferent* _referent)              { FRM_ASSERT(_referent); id = _referent->getID(); referent = _referent; }

	bool             isValid() const                                   { return id != 0u || referent != nullptr; }
	bool             isTransient() const                               { return id == 0u && referent != nullptr; }
	bool             isResolved() const                                { return referent != nullptr; }
	tReferent*       operator->()                                      { FRM_ASSERT(referent); return referent; }
	const tReferent* operator->() const                                { FRM_ASSERT(referent); return referent; }
	bool             operator==(const LocalReference& _rhs) const      { return id == _rhs.id && referent == _rhs.referent; }
	bool             operator!=(const LocalReference& _rhs) const      { return !(*this == _rhs); }

	bool             serialize(Serializer& _serializer_, const char* _name = nullptr);
};
using LocalNodeReference = LocalReference<SceneNode>;
using LocalComponentReference = LocalReference<Component>;

////////////////////////////////////////////////////////////////////////////////
// GlobalReference
// Reference an object within a scene subtree.
////////////////////////////////////////////////////////////////////////////////
template <typename tReferent>
struct GlobalReference
{
	SceneGlobalID    id;
	tReferent*       referent = nullptr;

	                 GlobalReference()                                                                   = default;
	                 GlobalReference(SceneGlobalID _id, tReferent* _referent = nullptr)                  { FRM_ASSERT(!referent || referent->getID() == _id.local); referent = _referent; id = _id; }
	                 GlobalReference(SceneID _sceneID, SceneID _localID, tReferent* _referent = nullptr) { FRM_ASSERT(!referent || referent->getID() == _localID); referent = _referent; id = { _sceneID, _localID }; }

	bool             isValid() const                                                                     { return id.local != 0u || referent != nullptr; }
	bool             isTransient() const                                                                 { return id.local == 0u && referent != nullptr; }
	bool             isResolved() const                                                                  { return referent != nullptr; }
	tReferent*       operator->()                                                                        { FRM_ASSERT(referent); return referent; }
	const tReferent* operator->() const                                                                  { FRM_ASSERT(referent); return referent; }
	bool             operator==(const GlobalReference& _rhs) const                                       { return id == _rhs.id && referent == _rhs.referent; }
	bool             operator!=(const GlobalReference& _rhs) const                                       { return !(*this == _rhs); }

	bool             serialize(Serializer& _serializer_, const char* _name = nullptr);
};
using GlobalNodeReference = GlobalReference<SceneNode>;
using GlobalComponentReference = GlobalReference<Component>;

////////////////////////////////////////////////////////////////////////////////
// World
// Context for a hierarchy of scenes. Contains a registry of scene instances.
////////////////////////////////////////////////////////////////////////////////
class World: public Serializable<World>
{
public:

	enum class UpdatePhase
	{
		GatherActive, // Flag active components for update.
		PrePhysics,   // Update pre-physics (e.g. animation).
		Hierarchy,    // Update transform hierarchy.
		Physics,      // Update physics (i.e. can run concurrently with physics).
		PostPhysics,  // Update post-physics (e.g. IK).
		PreRender,    // Last phase before the renderer, world transforms are finalized.

		_Count,
		All
	};

	enum class State
	{
		Init,
		PostInit,
		Shutdown,

		_Count,
		Deleted       // Set during dtor, useful for debugging.
	};

	// Create a new world. If _path, load from a .world file, else initialize default scene (simple camera).
	// Implicitly calls Use().
	static World*            Create(const char* _path = nullptr);

	// Increment refcount. Call init(), postInit(), return true if both succeed.
	static bool              Use(World* _world_);

	// Decrement refcount. Call shutdown() and destroy if refcount is 0.
	static void              Release(World*& _world_);

	// Get/set the current active world.
	static World*            GetCurrent()                 { return s_current; }
	static void              SetCurrent(World* _world)    { s_current = _world; } // \todo Need to unset/set active camera? Possibly need activation events?

	// Get the current draw/cull camera. These are convenience wrappers equivalent to World::GetCurrent()->get{Draw,Cull}CameraComponent()->getCamera().
	static Camera*           GetDrawCamera();
	static Camera*           GetCullCamera();
	
	// Update the root scene for the specified phase.
	void                     update(float _dt, UpdatePhase _phase = UpdatePhase::All);
	
	// Serialize world. Note that m_rootScene is serialized inline if it has no path.
	bool                     serialize(Serializer& _serializer_);

	// Standard lifetime methods.
	bool                     init();
	bool                     postInit();
	void                     shutdown();
	void                     reset();

	// Get/set the current draw/cull camera component. Getters will create a transient node with a camera component + free look controller if none exists.
	CameraComponent*         getDrawCameraComponent();
	void                     setDrawCameraComponent(CameraComponent* _cameraComponent);
	CameraComponent*         getCullCameraComponent();
	void                     setCullCameraComponent(CameraComponent* _cameraComponent);

	// Get/set the current 'input consumer' (e.g. player controller).
	Component*               getInputConsumer() const;
	void                     setInputConsumer(Component* _component);

	const PathStr&           getPath() const             { return m_path; }
	State                    getState() const            { return m_state; }
	Scene*                   getRootScene()              { return m_rootScene; }

private:

	using SceneList = eastl::fixed_vector<Scene*, 8>;
	using SceneInstanceMap = eastl::map<StringHash, SceneList>;

	static World*            s_current;
	int                      m_refCount          = 0;
	PathStr                  m_path              = "";
	State                    m_state             = State::Shutdown;
	Scene*                   m_rootScene         = nullptr;
	SceneInstanceMap         m_sceneInstances;

	GlobalComponentReference m_drawCamera;
	GlobalComponentReference m_cullCamera;
	GlobalComponentReference m_inputConsumer;

	// Destroy a world instance. Called implicitly by Release().
	static void              Destroy(World*& _world_);

	                         World();
	                         ~World();

	// Add/remove a scene instance to/from the scene instance map.
	void                     addSceneInstance(Scene* _scene);
	void                     removeSceneInstance(Scene* _scene);

	// Called by GetDrawCamera()/GetCullCamera(). Search the scene hierarchy for an active CameraComponent instance, else create a transient node with a default camera.
	CameraComponent*         findOrCreateDefaultCamera();

	friend class Scene;
	friend class WorldEditor;
};

////////////////////////////////////////////////////////////////////////////////
// SceneNode
// Basic spatial scene unit + container for components.
// Scene nodes have 3 transforms:
//   - m_initial is the transform stored in the scene file. This is used to prime
//     m_local at the start of each update.
//   - m_local may be updated by kinematic animation or xform components during
//     PrePhysics.
//   - m_world is resolved by traversing the scene during the Hierarchy update.
//     It may be subsequently overridden by world space kinematic transforms or
//     physics components.
////////////////////////////////////////////////////////////////////////////////
class SceneNode: public Serializable<SceneNode>
{
public:

	enum class Flag
	{
		Active,    // Node is active, components will be gathered during GatherActive.
		Static,    // Node is static, world transform won't be updated after postInit(). See Component::isStatic().
		Transient, // Node is transient, won't be serialized and is automatically deleted during shutdown().

		BIT_FLAGS_COUNT_DEFAULT(Active)
	};
	using Flags = BitFlags<Flag>;

	enum class Event
	{
		OnInit,
		OnPostInit,
		OnShutdown,
		OnDestroy,
		OnEdit,

		_Count
	};
	
	typedef void (Callback)(SceneNode* _node, void* _arg);

	// Update for the specified phase.
	void                      update(float _dt, World::UpdatePhase _phase);

	// Serialize the node.
	bool                      serialize(Serializer& _serializer_);

	// Standard lifetime methods.
	bool                      init();
	bool                      postInit();
	void                      shutdown();
	void                      reset();

	// Add/remove a component. The node takes ownership of the component's memory (client code should *not* call Component::Destroy()).
	void                      addComponent(Component* _component);
	void                      removeComponent(Component* _component);

	// Find a component by class name. Return 0 if not found.
	Component*                findComponent(StringHash _className);

	// Register/unregister a callback. _arg_ will typically be a ptr to the calling component.
	void                      registerCallback(SceneNode::Event _event, Callback* _callback, void* _arg_);
	void                      unregisterCallback(SceneNode::Event _event, Callback* _callback, void* _arg_);

	// Set the parent node. Preserves world space transform. Note that the parent may *not* be transient unless this node is also transient.
	void                      setParent(SceneNode* _parent_);

	// Add a child node. Equivalent to _child_->setParent().
	void                      addChild(SceneNode* _child_);

	// Set child scene.
	void                      setChildScene(Scene* _scene_);

	SceneID                   getID() const                       { return m_id; }
	bool                      getFlag(Flag _flag) const           { return m_flags.get(_flag); }
	void                      setFlag(Flag _flag, bool _value);
	bool                      isActive() const                    { return m_flags.get(Flag::Active); }
	bool                      isStatic() const                    { return m_flags.get(Flag::Static); }
	bool                      isTransient() const                 { return m_flags.get(Flag::Transient); }
	World::State              getState() const                    { return m_state; }
	const char*               getName() const                     { return m_name.c_str(); }
	const mat4&               getInitial() const                  { return m_initial; }
	void                      setInitial(const mat4& _initial)    { m_initial = _initial; }
	const mat4&               getLocal() const                    { return m_local; }
	void                      setLocal(const mat4& _local)        { m_local = _local; }
	const mat4&               getWorld() const                    { return m_world; }
	void                      setWorld(const mat4& _world)        { m_world =  _world; }
	vec3                      getPosition() const                 { return GetTranslation(m_world); }
	vec3                      getForward() const                  { return m_world[2].xyz(); }
	Scene*                    getParentScene() const              { return m_parentScene; }
	Scene*                    getChildScene() const               { return m_childScene; }
	World*                    getParentWorld() const; // \todo Decl order issue, need to reorder code...

private:

	struct CallbackListEntry
	{
		Callback* func;
		void*     arg;

		bool operator==(const CallbackListEntry& _rhs) { return _rhs.func == func && _rhs.arg == arg; }
		void operator()(SceneNode* _node)              { func(_node, arg); }
	};

	using ChildList     = eastl::fixed_vector<LocalNodeReference, 1>;
	using ComponentList = eastl::fixed_vector<LocalComponentReference, 2>;
	using CallbackList  = eastl::fixed_vector<CallbackListEntry, 1>;

	SceneID                   m_id            = 0u;
	Flags                     m_flags         = Flags();
	World::State              m_state         = World::State::Shutdown;
	String<24>                m_name          = "";
	mat4                      m_initial       = identity;
	mat4                      m_local         = identity;
	mat4                      m_world         = identity;
	Scene*                    m_parentScene   = nullptr;
	Scene*                    m_childScene    = nullptr;
	LocalNodeReference        m_parent;
	ChildList                 m_children;
	ComponentList             m_components;
	CallbackList              m_callbacks[(int)Event::_Count];

	                          SceneNode(Scene* _parentScene, SceneID _id, const char* _name = nullptr);
	                          ~SceneNode();

	// Dispatch all callbacks for the specified event.
	void                      dispatchCallbacks(Event _event);

	// Set the static flag per Component::isStatic() (node is static if *all* components are static).
	void                      updateStaticState();

	// Child list helpers.
	void                      removeChild(SceneNode* _child_);
	ChildList::iterator       findChild(const SceneNode* _child);

	friend class Pool<SceneNode>; // workaround for private ctor/dtor
	friend class Scene;
	friend class WorldEditor;
};

////////////////////////////////////////////////////////////////////////////////
// Scene
// Context for a hierarchy of nodes.
//
// Nodes may contain a child scene instance; nodes in a child scene are 
// referencable via the global node map. Note that parent -> child relationships 
// between nodes may *not* cross a scene boundary.
//
// \todo
// - More efficient updates to global node map (insert/remove a single object
//   rather than rebuilding the whole thing).
////////////////////////////////////////////////////////////////////////////////
class Scene: public Serializable<Scene>
{
public:

	// Recursively update the scene for the specified phase.
	void                       update(float _dt, World::UpdatePhase _phase);

	// Serialize the scene.
	bool                       serialize(Serializer& _serializer_);

	// Standard lifetime methods.
	bool                       init();
	bool                       postInit();
	void                       shutdown();
	void                       reset();

	// Create a new permanent node. Unlike transient nodes, these are serialized.
	// init() and postInit() must subsequently be called on the node.
	// _localID must be unique within the scene; use findUniqueNodeID().
	SceneNode*                 createNode(SceneID _localID, const char* _name = nullptr, SceneNode* _parent = nullptr);

	// Create a new transient node. Unlike permanent nodes, these are not serialized.
	// init() and postInit() must subsequently be called on the node.
	SceneNode*                 createTransientNode(const char* _name = nullptr, SceneNode* _parent = nullptr);

	// Destroy _node_.
	// Note that deletes are deferred until the next update().
	void                       destroyNode(SceneNode* _node_);

	// Depth-first traversal of the scene starting at _root, call _onVisit for each node. 
	// Traversal proceeds to a node's children only if _onVisit returns true.
	void                       traverse(const eastl::function<bool(SceneNode*)>& _onVisit, SceneNode* _root = nullptr);

	// Resolve a reference (global or local). Return true if the referent was successfully set.
	template <typename tReferent>
	bool                       resolveReference(tReferent& _ref_);

	// Find a node given the local/global ID. Return nullptr if not found.
	SceneNode*                 findNode(SceneID _localID, SceneID _sceneID = SceneID()) const;

	// Find a component given the local/global ID. Return nullptr if not found.
	Component*                 findComponent(SceneID _localID, SceneID _sceneID = SceneID()) const;

	// Find a unique node ID (max of all current IDs + 1).
	SceneID                    findUniqueNodeID() const;

	// Find a unique component ID (max of all component IDs + 1).
	SceneID                    findUniqueComponentID() const;

	void                       setPath(const char* _path);
	const PathStr&             getPath() const                 { return m_path; }
	SceneNode*                 getRootNode() const             { return m_root.referent; }
	SceneNode*                 getParentNode() const           { return m_parentNode; }
	World*                     getParentWorld() const          { return m_world; }
	World::State               getState() const                { return m_state; }

	// Find a global reference relative to this scene given _referent.
	template <typename tReferent>
	GlobalReference<tReferent> findGlobal(const tReferent* _referent) const;

private:

	using NodePool             = Pool<SceneNode>;
	using LocalNodeMap         = eastl::map<SceneID, SceneNode*>;
	using LocalComponentMap    = eastl::map<SceneID, Component*>;
	using GlobalNodeMap        = eastl::map<SceneGlobalID, SceneNode*>;
	using GlobalComponentMap   = eastl::map<SceneGlobalID, Component*>;

	PathStr                    m_path             = "";                     // Empty if not from a file.
	World::State               m_state            = World::State::Shutdown;
	World*                     m_world            = nullptr;                // World context.
	SceneNode*                 m_parentNode       = nullptr;                // Owning parent node.
	LocalNodeReference         m_root;                                      // Root node.
	NodePool                   m_nodePool;
	LocalNodeMap               m_localNodeMap;
	LocalComponentMap          m_localComponentMap;
	GlobalNodeMap              m_globalNodeMap;
	GlobalComponentMap         m_globalComponentMap;
	eastl::vector<SceneNode*>  m_pendingDeletes;

	static Scene*              CreateDefault(World* _world);

	                           Scene(World* _world, SceneNode* _parentNode = nullptr);
	                           ~Scene();

	// Component list helpers (only called by ScenenNode).
	void                       addComponent(Component* _component);
	void                       removeComponent(Component* _component);

	// Init the global node/component reference maps.
	void                       initGlobalReferenceMap();

	// Recursively re-init node/component global reference maps up the scene hierarchy.
	void                       resetGlobalReferenceMap();

	// Shutdown/delete any pending nodes.
	void                       flushPendingDeletes();

	friend class SceneNode;
	friend class World;
	friend class WorldEditor;
};

inline World* SceneNode::getParentWorld() const { return m_parentScene->getParentWorld(); }

} // namespace frm
