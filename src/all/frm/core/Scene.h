#pragma once

#include <frm/core/def.h>
#include <frm/core/math.h>

#include <apt/Pool.h>
#include <apt/String.h>

#include <EASTL/vector.h>

#define frm_Scene_ENABLE_EDIT

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Node
// Basic scene unit; comprises a local/world matrix, metadata and hierarchical
// information.
// \note Don't create loops in the hiearchy.
////////////////////////////////////////////////////////////////////////////////
class Node
{
	friend class apt::Pool<Node>;
	friend class Scene;
public:
	typedef apt::String<24> NameStr;
	typedef uint64 Id;
	static const Id kInvalidId = ~0u;

	enum Type
	{
		Type_Root,
		Type_Camera,
		Type_Object,

		Type_Count
	};

	enum State
	{
		State_Active   = 1 << 0, // Enable/disable update.
		State_Dynamic  = 1 << 1, // Update every frame (else static).
		State_Selected = 1 << 3, // Activates certain xforms/components (e.g. which receive input).

		State_Any = 0xff
	};

	Id           getId() const                       { return m_id; }
	const char*  getName() const                     { return (const char*)m_name; }
	void         setName(const char* _name)          { m_name.set(_name); }
	void         setNamef(const char* _fmt, ...);

	Type         getType() const                     { return (Type)m_type; }
	void         setType(Type _type)                 { m_type = _type; }
	
	uint8        getStateMask() const                { return m_state; }
	void         setStateMask(uint8 _mask)           { m_state = _mask; }
	bool         isActive() const                    { return (m_state & State_Active) != 0; }
	void         setActive(bool _state)              { m_state = _state ? (m_state | State_Active) : (m_state & ~State_Active); }
	bool         isDynamic() const                   { return (m_state & State_Dynamic) != 0; }
	void         setDynamic(bool _state)             { m_state = _state ? (m_state | State_Dynamic) : (m_state & ~State_Dynamic); }
	bool         isStatic() const                    { return !isDynamic(); }
	void         setStatic(bool _state)              { setDynamic(!_state); }
	bool         isSelected() const                  { return (m_state & State_Selected) != 0; }
	void         setSelected(bool _state)            { m_state = _state ? (m_state | State_Selected) : (m_state & ~State_Selected); }
	
	uint64       getUserData() const                 { return m_userData; }
	void         setUserData(uint64 _data)           { m_userData = _data; }

	uint64       getSceneData() const                { return m_sceneData; }
	Camera*      getSceneDataCamera() const          { APT_ASSERT(m_type == Type_Camera); return (Camera*)m_sceneData; }
	Scene*       getSceneDataScene() const           { APT_ASSERT(m_type == Type_Root); return (Scene*)m_sceneData;    }

	const mat4&  getLocalMatrix() const              { return m_localMatrix; }
	void         setLocalMatrix(const mat4& _mat)    { m_localMatrix = _mat; }
	vec3         getLocalPosition() const            { return m_localMatrix[3].xyz(); }
	void         setLocalPosition(const vec3& _p)    { m_localMatrix[3] = vec4(_p, 1.0f); }
	
	const mat4&  getWorldMatrix() const              { return m_worldMatrix; }
	void         setWorldMatrix(const mat4& _mat)    { m_worldMatrix = _mat; }
	vec3         getWorldPosition() const            { return m_worldMatrix[3].xyz(); }
	void         setWorldPosition(const vec3& _p)    { m_worldMatrix[3] = vec4(_p, 1.0f); }
	
	
	void         addXForm(XForm* _xform);
	void         removeXForm(XForm* _xform);
	int          getXFormCount() const               { return (int)m_xforms.size(); }
	XForm*       getXForm(int _i)                    { return m_xforms[_i]; }
	void         moveXForm(const XForm* _xform, int _dir);

	void         addComponent(Component* _component);
	void         removeComponent(Component* _component);
	int          getComponentCount() const           { return (int)m_components.size(); }
	Component*   getComponent(int _i)                { return m_components[_i]; }

	Node*        getParent()                         { return m_parent; }
	void         setParent(Node* _node);
	int          getChildCount() const               { return (int)m_children.size(); }
	Node*        getChild(int _i)                    { return m_children[_i]; }
	void         addChild(Node* _node);
	void         removeChild(Node* _node);

	friend bool  Serialize(apt::Serializer& _serializer_, Scene& _scene_, Node& _node_);

private:
 // meta
	Id                    m_id;          // Unique id.
	NameStr               m_name;        // User-friendly name (not necessarily unique).
	Type                  m_type;        // Type.
	uint8                 m_state;       // State mask.
	uint64                m_userData;    // Application-defined node data.
	uint64                m_sceneData;   // Scene-defined data.

 // spatial
	mat4                  m_localMatrix; // Initial (local) transformation.
	mat4                  m_worldMatrix; // Final transformation with any xforms/components applied.
	eastl::vector<XForm*> m_xforms;      // XForm list (applied in order). Node owns the xform memory.

 // components
	eastl::vector<Component*> m_components; // Component list (updated in order). Node owns the component memory.

 // hierarchy
	Node*                 m_parent;
	eastl::vector<Node*>  m_children;

	// Auto name based on type, e.g. Camera_001, Object_123
	static void AutoName(Node::Type _type, Node::NameStr& out_);
	
	// Recursively update _node_, apply xforms.
	static void Update(Node* _node_, float _dt, uint8 _stateMask);

	Node();
	Node(Type _type, Id _id, uint8 _state, const char* _name = nullptr);

	// Maintains traversability by reparenting child nodes to m_parent.	
	~Node();

	// Move _ith XForm within the stack; _dir is an offset from the current index. Return new index.
	int moveXForm(int _i, int _dir);


	void setSceneDataCamera(Camera* _camera) { APT_ASSERT(m_type == Type_Camera); m_sceneData = (uint64)_camera; }
	void setSceneDataScene(Scene* _scene)    { APT_ASSERT(m_type == Type_Root);   m_sceneData = (uint64)_scene;  }

}; // class Node


////////////////////////////////////////////////////////////////////////////////
// Scene
////////////////////////////////////////////////////////////////////////////////
class Scene
{
public:
	typedef bool (OnVisit)(Node* _node_);

	static Scene*  GetCurrent()                      { return s_currentScene; }
	static void    SetCurrent(Scene* _scene)         { s_currentScene = _scene; }

	static Camera* GetDrawCamera()                   { return s_currentScene->getDrawCamera(); }
	static Camera* GetCullCamera()                   { return s_currentScene->getCullCamera(); }

	// Load scene from path, swap with scene_ if successful & return true.
	static bool Load(const char* _path, Scene& scene_);

	// Save scene to path, return true if successful.
	static bool Save(const char* _path, Scene& _scene);

	Scene();
	~Scene();

	// Update all nodes matching _stateMask. If a node does not match _stateMask 
	// then none of its children are updated.
	void update(float _dt, uint8 _stateMask = Node::State_Active | Node::State_Dynamic);

	// Pre-order traversal of the node graph starting at _root_, calling _callback 
	// at every node which matches _stateMask. The callback should return false if 
	// the traversal should stop.
	bool traverse(Node* _root_, uint8 _stateMask, OnVisit* _callback);

	Node*   createNode(Node::Type _type, Node* _parent = nullptr);
	void    destroyNode(Node*& _node_);
	Node*   findNode(Node::Id _id, Node::Type _typeHint = Node::Type_Count);
	Node*   findNode(const char* _name, Node::Type _typeHint = Node::Type_Count);
	int     getNodeCount(Node::Type _type) const    { return (int)m_nodes[_type].size(); }
	Node*   getNode(Node::Type _type, int _i) const { return m_nodes[_type][_i]; }
	Node*   getRoot()                               { return m_root; }

	// Create a camera with parameters from _copyFrom, plus a new camera node.
	Camera* createCamera(const Camera& _copyFrom, Node* _parent = nullptr);
	void    destroyCamera(Camera*& _camera_);
	int     getCameraCount() const                  { return (int)m_cameras.size(); }
	Camera* getCamera(int _i) const                 { return m_cameras[_i]; }
	Camera* getDrawCamera() const                   { return m_drawCamera; }
	void    setDrawCamera(Camera* _camera)          { m_drawCamera = _camera; }
	Camera* getCullCamera() const                   { return m_cullCamera; }
	void    setCullCamera(Camera* _camera)          { m_cullCamera = _camera; }

	// \note Node names beginning with '#' are ignored during serialization (use for any nodes added programmatcially).
	friend bool Serialize(apt::Serializer& _serializer_, Scene& _scene_);
	friend bool Serialize(apt::Serializer& _serializer_, Scene& _scene_, Node& _node_);
#ifdef frm_Scene_ENABLE_EDIT
	void edit();

	// ui helpers; call beginSelect*() followed by select*()
	void      beginSelectNode();
	Node*     selectNode(Node* _current, Node::Type _type = Node::Type_Count);
	void      beginSelectCamera();
	Camera*   selectCamera(Camera* _current);
#endif
	
	friend void swap(Scene& _a, Scene& _b);

private:
	static Scene*           s_currentScene;

 // nodes
	Node::Id                m_nextNodeId = 0;           // Monotonically increasing id for nodes.
	Node*                   m_root = nullptr;           // Everything is a child of root.              
	eastl::vector<Node*>    m_nodes[Node::Type_Count];  // Nodes binned by type.
	apt::Pool<Node>         m_nodePool;

 // cameras
	Camera*                 m_drawCamera = nullptr;
	Camera*                 m_cullCamera = nullptr;
	eastl::vector<Camera*>  m_cameras;
	apt::Pool<Camera>       m_cameraPool;

#ifdef frm_Scene_ENABLE_EDIT
	bool       m_showNodeGraph3d    = false;
	Node*      m_editNode           = nullptr;
	Node*      m_storedNode         = nullptr;
	XForm*     m_editXForm          = nullptr;
	Component* m_editComponent      = nullptr;
	Camera*    m_editCamera         = nullptr;
	Camera*    m_storedCullCamera   = nullptr;
	Camera*    m_storedDrawCamera   = nullptr;

	void       editNodes();
	void       editCameras();

	void       beginCreateNode();
	Node*      createNode(Node* _current);

	void       beginCreateXForm();
	XForm*     createXForm(XForm* _current);

	void       beginCreateComponent();
	Component* createComponent(Component* _current);

	void       drawHierarchy(Node* _node);
#endif

}; // class Scene

} // namespace frm
