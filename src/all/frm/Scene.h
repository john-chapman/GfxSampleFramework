#pragma once
#ifndef frm_Scene_h
#define frm_Scene_h
/*	Purpose of the scene graph is to maintain spatial/hierarchical information and allow
	this to be coupled to render data, therfore each 'node' in the hierarchy must finally
	provide a world matrix, plus some other information about how to draw the node (in
	practice a a node 'type' + userdata param will be enough).

	Node updates: during update, first 'reset' the world matrix with the local matrix, then
	apply xforms, then apply the parent's world matrix, then update the children. In theory
	xforms could be split into 'local' (applied first) and 'world' (applied after the parent's
	transform), but it's probably overkill here. 

	Xforms are attached to one node only (in order to make lifetime management easier - no
	refcounting). Some XForms may support 'OnComplete' behavior and call a callback or 
	delete themselves when done, which allows for simple chaining and animation. Note that
	not all xforms need support callbacks, as long as each XForm has a reference to its
	node. In practice, due to the serialization requirement, callbacks will need to be
	named and registered in a global table somewhere (then you can just store the name).

	The current scene should be globally accessible to make access to the current camera/cull
	camera easier. 

	The scene graph must be serializable to json - this puts limits on what you can easily
	do with the XForms - saving the scene state at any point should just work, meaning you
	can't really delete XForms when they're complete. Probably this is fine, though - just
	let them go 'inactive'. For more complex stuff you could use lua directly.

	Stored, re-usable scenes are not really useful without a default renderer. There should
	be at least an 'DrawResource'-type specification which can be reused, for example:

	- 'Objects' are a set of LODs, each LOD is a separate Drawable.
	- 'Drawables' (better name?) are Mesh + Material array, + render flags 
		(i.e. cast shadows, cast reflections, static, dynamic, etc.). Material is an 
		array because Mesh may contain submeshes.
	- 'Materials' are shaders (with defines set according to the render flags), some per-
	   material static data (constant buffer), plus per-instance data (constant buffer).
	- Renderer executes a geometry pass by: LOD select + cull all objects according to the flags/
	  spatial data, sort draw list (save draw lists for VR?)
	- Lights are basic points/spot/omni with some properties/flags.
*/

#include <frm/def.h>
#include <frm/math.h>

#include <apt/Pool.h>
#include <apt/String.h>

#include <vector>

namespace frm {

class Camera;
class XForm;

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


	const mat4&  getLocalMatrix() const              { return m_localMatrix; }
	void         setLocalMatrix(const mat4& _mat)    { m_localMatrix = _mat; }
	
	const mat4&  getWorldMatrix() const              { return m_worldMatrix; }
	void         setWorldMatrix(const mat4& _mat)    { m_worldMatrix = _mat; }
	
	int          getXFormCount() const               { return (int)m_xforms.size(); }
	XForm*       getXForm(int _i)                    { return m_xforms[_i]; }

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

 // spatial
	mat4                m_localMatrix; //< Initial (local) transformation.
	mat4                m_worldMatrix; //< Final transformation with any XForms applied.
	std::vector<XForm*> m_xforms;      //< XForm list (applied in order).

 // hierarchy
	Node*               m_parent;      //< Parent node.
	std::vector<Node*>  m_children;    //< Child nodes.


	Node(
		Id          _id, 
		const char* _name, 
		Type        _type, 
		uint8       _stateMask, 
		uint64      _userData
		);

	/// Maintains traversability by reparenting child nodes to m_parent.	
	~Node();

	/// Move _ith XForm within the stack; _dir is an offset from the current index.
	/// \return new index.
	int moveXForm(int _i, int _dir);

	/// Auto name based on type, e.g. Camera_001, Object_123
	static void AutoName(Type _type, NameStr& out_);

}; // class Node


////////////////////////////////////////////////////////////////////////////////
/// \class Scene
/// Manages nodes/cameras. 
////////////////////////////////////////////////////////////////////////////////
class Scene
{
public:
	typedef bool (OnVisit)(Node* _node_);

	static Scene& GetCurrent();
	static void   SetCurrent(Scene& _scene);

	Scene();
	~Scene();

	/// Update all nodes matching _stateMask. If a node does not match _stateMask 
	/// then none of its children are updated.
	void update(float _dt, uint8 _stateMask = Node::kStateActive | Node::kStateDynamic);

	/// Pre-order traversal of the node graph starting at _root_, calling _callback 
	/// at every node which matches _stateMask. The callback should return false if 
	/// the traversal should stop.
	bool traverse(OnVisit* _callback, Node* _root_, uint8 _stateMask = Node::kStateAny);

private:
 // nodes
	static Node::Id         s_nextNodeId;               //< Monotonically increasing id for nodes.
	Node                    m_root;                     //< Everything is a child of root.              
	std::vector<Node*>      m_nodes[Node::kTypeCount];  //< Nodes binned by type.
	apt::Pool<Node>         m_nodePool;

 // cameras
	Camera*                 m_drawCamera;
	Camera*                 m_cullCamera;
	std::vector<Camera*>    m_cameras;
	apt::Pool<Camera>       m_cameraPool;

}; // class Scene


} // namespace frm







// -- old
#include <frm/def.h>
#include <frm/math.h>

#include <apt/Pool.h>
#include <apt/String.h>
#include <apt/StringHash.h>

#include <vector>

#define frm_Scene_ENABLE_EDIT

namespace old {
namespace frm
{

////////////////////////////////////////////////////////////////////////////////
/// \class Scene
/// \note find() and destroyNode() methods will be fairly slow as they just
///   do a linear search of the node bins.
////////////////////////////////////////////////////////////////////////////////
class Scene
{
	friend class SceneEditor;
public:
	typedef bool (VisitNode)(Node* _node_);

	Scene();
	~Scene();

	/// Update all nodes in the hierarchy who match _stateMask. If a node does
	/// not match _stateMask then none of its children are updated.
	void update(float _dt, uint8 _stateMask = Node::kStateActive | Node::kStateDynamic);

	/// Traverse the node graph starting at _root_, calling _callback at every node 
	/// which matches _stateMask. VisitNodeCallback should return false if the traversal
	/// should stop.
	bool traverse(VisitNode* _callback, Node* _root_, uint8 _stateMask = Node::kStateAny);

	/// Create a new node, return a ptr to it.
	/// \note The ptr is guranteed to be persistent for the lifetime of the scene.
	Node* createNode(const char* _name, Node::Type _type, Node* _parent_ = 0, const void* _userData = 0);

	/// Destroy _node_.
	void destroyNode(Node*& _node_);

	/// Find the first node matching name, return 0 if no match was found.
	/// \note Names are not guaranteed to be unique, however you can build a list
	///   of all matching nodes by repeatedly calling findNode() and passing the
	///   previous result to the _start param until the function returns 0. Very
	///   stupid, very slow method, use sparingly (i.e. for UI purposes).
	Node* findNode(const char* _name, const Node* _start = 0);

	/// Find node matching _id, return 0 if no match was found. Ids are 
	/// guaranteed to be unique.
	Node* findNode(Node::Id _id);


	int         getNodeCount(Node::Type _type) const  { return (int)m_nodes[(int)_type].size(); }
	Node*       getNode(Node::Type _type, int _i)     { APT_ASSERT(_i < getNodeCount(_type)); return m_nodes[(int)_type][_i]; }
	const Node* getRoot() const                       { return &m_root; }


	Camera*     createCamera(const Camera& _copyFrom, Node* _parent = 0);
	void        destroyCamera(Camera*& _camera_);
	int         getCameraCount() const                { return (int)m_cameras.size(); }
	Camera*     getCamera(int _i)                     { APT_ASSERT(_i < getCameraCount()); return m_cameras[_i]; }
	Camera*     getDrawCamera() const                 { return m_drawCamera; }
	void        setDrawCamera(Camera* _camera)        { m_drawCamera = _camera; }
	Camera*     getCullCamera() const                 { return m_cullCamera; }
	void        setCullCamera(Camera* _camera)        { m_cullCamera = _camera; }
	
private:
	static Node::Id         s_nextNodeId;               //< Monotonically increasing id for nodes.
	Node                    m_root;                     //< Everything is a child of root.              
	std::vector<Node*>      m_nodes[Node::kTypeCount];  //< Nodes binned by type.
	apt::Pool<Node>         m_nodePool;

	std::vector<Camera*>    m_cameras;
	apt::Pool<Camera>       m_cameraPool;
	Camera*                 m_drawCamera;
	Camera*                 m_cullCamera;

	/// Recursive node update (called by update()).
	void update(Node* _node, float _dt, uint8 _stateMask);

}; // class Scene

#ifdef frm_Scene_ENABLE_EDIT

	class SceneEditor
	{
	public:
		SceneEditor(Scene* _scene_);

		void edit();

		Scene*   m_scene;
		Node*    m_editNode;
		Camera*  m_editCamera;
		XForm*   m_editXForm;

		bool     m_showNodeGraph3d; //< Show node graph in 3d viewport.

		void beginSelectNode();
		Node* selectNode(Node* _current, Node::Type _type = Node::kTypeCount);

		void beginSelectCamera();
		Camera* selectCamera(Camera* _current);

		void beginCreateNode();
		Node* createNode(Node* _current);

		void beginCreateXForm();
		XForm* createXForm(XForm* _current);

		void drawHierarchy(Node* _node, int _depth = 0);
	};

#endif // frm_Scene_ENABLE_EDIT

} // namespace frm

} // namespace old

#endif // frm_Scene_h
