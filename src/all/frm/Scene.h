#pragma once
#ifndef frm_Scene_h
#define frm_Scene_h

#include <frm/def.h>
#include <frm/math.h>

#include <apt/Pool.h>
#include <apt/String.h>

#include <vector>

#define frm_Scene_ENABLE_EDIT

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class Node
/// Basic scene unit; comprises a local/world matrix, metadata and hierarchical
/// information.
/// \note Don't create loops in the hiearchy.
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
		kTypeRoot,
		kTypeCamera,
		kTypeObject,
		kTypeLight,

		kTypeCount
	};

	enum State
	{
		kStateActive   = 1 << 0, //< Enable/disable update.
		kStateDynamic  = 1 << 1, //< Update every frame (else static).
		kStateSelected = 1 << 3, //< Activates certain xforms (e.g. which receive input).

		kStateAny = 0xff
	};

	Id           getId() const                       { return m_id; }
	const char*  getName() const                     { return m_name; }
	void         setName(const char* _name)          { m_name.set(_name); }
	void         setNamef(const char* _fmt, ...);

	Type         getType() const                     { return (Type)m_type; }
	void         setType(Type _type)                 { m_type = _type; }
	
	uint8        getStateMask() const                { return m_state; }
	void         setStateMask(uint8 _mask)           { m_state = _mask; }
	bool         isActive() const                    { return (m_state & kStateActive) != 0; }
	void         setActive(bool _state)              { m_state = _state ? (m_state | kStateActive) : (m_state & ~kStateActive); }
	void         setDynamic(bool _state)             { m_state = _state ? (m_state | kStateDynamic) : (m_state & ~kStateDynamic); }
	void         setStatic(bool _state)              { setDynamic(!_state); }
	void         setSelected(bool _state)            { m_state = _state ? (m_state | kStateSelected) : (m_state & ~kStateSelected); }
	bool         isDynamic() const                   { return (m_state & kStateDynamic) != 0; }
	bool         isStatic() const                    { return !isDynamic(); }
	bool         isSelected() const                  { return (m_state & kStateSelected) != 0; }
	
	uint64       getUserData() const                 { return m_userData; }
	void         setUserData(uint64 _data)           { m_userData = _data; }

	uint64       getSceneData() const                { return m_sceneData; }
	Camera*      getSceneDataCamera() const          { APT_ASSERT(m_type == kTypeCamera); return (Camera*)m_sceneData; }
	Scene*       getSceneDataScene() const           { APT_ASSERT(m_type == kTypeRoot); return (Scene*)m_sceneData; }

	const mat4&  getLocalMatrix() const              { return m_localMatrix; }
	void         setLocalMatrix(const mat4& _mat)    { m_localMatrix = _mat; }
	
	const mat4&  getWorldMatrix() const              { return m_worldMatrix; }
	void         setWorldMatrix(const mat4& _mat)    { m_worldMatrix = _mat; }
	
	void         addXForm(XForm* _xform);
	void         removeXForm(XForm* _xform);
	int          getXFormCount() const               { return (int)m_xforms.size(); }
	XForm*       getXForm(int _i)                    { return m_xforms[_i]; }
	void         moveXForm(const XForm* _xform, int _dir);

	Node*        getParent()                         { return m_parent; }
	void         setParent(Node* _node);
	int          getChildCount() const               { return (int)m_children.size(); }
	Node*        getChild(int _i)                    { return m_children[_i]; }
	void         addChild(Node* _node);
	void         removeChild(Node* _node);

private:
 // meta
	Id                  m_id;          //< Unique id.
	NameStr             m_name;        //< User-friendly name (not necessarily unique).
	uint8               m_type;        //< Node type (see Type enum).
	uint8               m_state;       //< State mask (see State enum).
	uint64              m_userData;    //< Application-defined node data.
	uint64              m_sceneData;   //< Scene-defined data.

 // spatial
	mat4                m_localMatrix; //< Initial (local) transformation.
	mat4                m_worldMatrix; //< Final transformation with any XForms applied.
	std::vector<XForm*> m_xforms;      //< XForm list (applied in order).

 // hierarchy
	Node*               m_parent;      //< Parent node.
	std::vector<Node*>  m_children;    //< Child nodes.


	Node(Id _id);

	/// Maintains traversability by reparenting child nodes to m_parent.	
	~Node();

	/// Move _ith XForm within the stack; _dir is an offset from the current index.
	/// \return new index.
	int moveXForm(int _i, int _dir);

	void setSceneDataCamera(Camera* _camera) { APT_ASSERT(m_type == kTypeCamera); m_sceneData = (uint64)_camera; }
	void setSceneDataScene(Scene* _scene)    { APT_ASSERT(m_type == kTypeRoot);   m_sceneData = (uint64)_scene; }

}; // class Node


////////////////////////////////////////////////////////////////////////////////
/// \class Scene
/// Manages scene objects (nodes, cameras, etc.).
////////////////////////////////////////////////////////////////////////////////
class Scene
{
public:
	typedef bool (OnVisit)(Node* _node_);

	static Scene&  GetCurrent()                      { return *s_currentScene; }
	static void    SetCurrent(Scene& _scene)         { s_currentScene = &_scene; }

	static Camera* GetDrawCamera()                   { return s_currentScene->getDrawCamera(); }
	static Camera* GetCullCamera()                   { return s_currentScene->getCullCamera(); }

	/// Load scene from path, swap with scene_ if successful & return true.
	static bool Load(const char* _path, Scene& scene_);

	/// Save scene to path, return true if successful.
	static bool Save(const char* _path, Scene& _scene);

	Scene();
	~Scene();

	/// Update all nodes matching _stateMask. If a node does not match _stateMask 
	/// then none of its children are updated.
	void update(float _dt, uint8 _stateMask = Node::kStateActive | Node::kStateDynamic);

	/// Pre-order traversal of the node graph starting at _root_, calling _callback 
	/// at every node which matches _stateMask. The callback should return false if 
	/// the traversal should stop.
	bool traverse(Node* _root_, uint8 _stateMask, OnVisit* _callback);

	Node*   createNode(Node::Type _type, Node* _parent = nullptr);
	void    destroyNode(Node*& _node_);
	Node*   findNode(Node::Id _id, Node::Type _typeHint = Node::kTypeCount);
	int     getNodeCount(Node::Type _type) const    { return (int)m_nodes[_type].size(); }
	Node*   getNode(Node::Type _type, int _i) const { return m_nodes[_type][_i]; }
	Node*   getRoot()                               { return m_root; }

	/// Create a camera with parameters from _copyFrom, plus a new camera node.
	Camera* createCamera(const Camera& _copyFrom, Node* _parent = nullptr);
	void    destroyCamera(Camera*& _camera_);
	int     getCameraCount() const                  { return (int)m_cameras.size(); }
	Camera* getCamera(int _i) const                 { return m_cameras[_i]; }
	Camera* getDrawCamera() const                   { return m_drawCamera; }
	void    setDrawCamera(Camera* _camera)          { m_drawCamera = _camera; }
	Camera* getCullCamera() const                   { return m_cullCamera; }
	void    setCullCamera(Camera* _camera)          { m_cullCamera = _camera; }

	/// Serialize to json.
	/// \note Node names beginning with '#' are ignored during serialization (use
	///   for any nodes added programmatcially).
	bool    serialize(apt::JsonSerializer& _serializer_);
	bool    serialize(apt::JsonSerializer& _serializer_, Node& _node_);
#ifdef frm_Scene_ENABLE_EDIT
	void edit();

	// ui helpers; call beginSelect*() followed by select*()
	void      beginSelectNode();
	Node*     selectNode(Node* _current, Node::Type _type = Node::kTypeCount);
	void      beginSelectCamera();
	Camera*   selectCamera(Camera* _current);
#endif
	
	friend void swap(Scene& _a, Scene& _b);

private:
	static Scene*           s_currentScene;

 // nodes
	Node::Id                m_nextNodeId;               //< Monotonically increasing id for nodes.
	Node*                   m_root;                     //< Everything is a child of root.              
	std::vector<Node*>      m_nodes[Node::kTypeCount];  //< Nodes binned by type.
	apt::Pool<Node>         m_nodePool;

 // cameras
	Camera*                 m_drawCamera;
	Camera*                 m_cullCamera;
	std::vector<Camera*>    m_cameras;
	apt::Pool<Camera>       m_cameraPool;

	/// Recursive update, called by update().
	void update(Node* _node_, float _dt, uint8 _stateMask);
	
	/// Auto name based on type, e.g. Camera_001, Object_123
	static void AutoName(Node::Type _type, Node::NameStr& out_);


#ifdef frm_Scene_ENABLE_EDIT
	bool      m_showNodeGraph3d;
	Node*     m_editNode;
	XForm*    m_editXForm;
	Camera*   m_editCamera;

	void      editNodes();
	void      editCameras();

	void      beginCreateNode();
	Node*     createNode(Node* _current);

	void      beginCreateXForm();
	XForm*    createXForm(XForm* _current);

	void      drawHierarchy(Node* _node);
#endif

}; // class Scene

} // namespace frm

#endif // frm_Scene_h
