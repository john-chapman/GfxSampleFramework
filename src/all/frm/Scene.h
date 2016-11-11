#pragma once
#ifndef frm_Scene_h
#define frm_Scene_h

#include <frm/def.h>
#include <frm/math.h>

#include <apt/Pool.h>
#include <apt/String.h>
#include <apt/StringHash.h>

#include <vector>

#define frm_Scene_ENABLE_EDIT

namespace frm
{

class XForm;

////////////////////////////////////////////////////////////////////////////////
/// \class Node
/// Basic scene unit; comprises a local/world matrix, metadata and hierarchical
/// information. Nodes may only be indirectly created via a Scene object.
/// \note Don't create loops in the hieararchy!
/// \note XForms may be shared between nodes - this allows additional 
///    flexibility, e.g. attach multiple nodes to an input controller without 
///    creating a common root node.
/// \note All operations maintain the traversability of the hierarchy, it 
///    shouldn't be possible to 'orphan' a node.
/// \todo const-ness is incosistent - you can implicitly modify a Node via a 
///    const ptr to it:
///    const Node* n;
///    n->getChild(0)->setParent(x); // modifies n!
////////////////////////////////////////////////////////////////////////////////
class Node
{
	friend class apt::Pool<Node>;
	friend class Scene;
	friend class SceneEditor;
public:
	typedef apt::String<24> NameStr;
	typedef uint64 Id;

	enum State
	{
		kStateActive   = 1 << 0, //< Enable/disable update.
		kStateStatic   = 1 << 1, //< Update manually or only on load.
		kStateDynamic  = 1 << 2, //< Update every frame.
		kStateSelected = 1 << 3, //< Activates certain xforms (e.g. which receive input).

		kStateAny = 0xff
	};

	enum Type
	{
		kTypeRoot,
		kTypeCamera,
		kTypeLight,
		kTypeObject,

		kTypeCount
	};

	/// Apply transforms to construct the local matrix at this node.
	void        applyXForms(float _dt);

	/// Add a transform to the top of the transform stack. Transforms may be shared
	/// between nodes.
	void        addXForm(XForm* _xform);
	/// Remove a transform from the stack. 
	void        removeXForm(XForm* _xform);
	
	/// Modify the parent-child hierachy. Both ends of the relationship are updated
	/// (i.e. addChild() set's _child's m_parent to this, etc.).
	/// \note Don't create loops in the hiearchy!
	void        setParent(Node* _parent);
	void        addChild(Node* _child);
	void        removeChild(Node* _child);

	void        setName(const char* _name)         { m_name.set(_name); }
	void        setNamef(const char* _fmt, ...);
	void        setLocalMatrix(const mat4& _local) { m_localMatrix = _local; }
	void        setWorldMatrix(const mat4& _world) { m_worldMatrix = _world; }
	void        setType(Type _type)                { m_type = (uint8)_type; }
	void        setUserData(const void* _userData) { m_userData = _userData; }

	const char* getName() const                    { return (const char*)m_name; }
	Id          getId() const                      { return m_id; }
	const mat4& getLocalMatrix() const             { return m_localMatrix; }
	const mat4& getWorldMatrix() const             { return m_worldMatrix; }
	Type        getType() const                    { return (Type)m_type; }
	uint8       getStateMask() const               { return m_stateMask; }
	const void* getUserData() const                { return m_userData; }
	Node*       getParent() const                  { return m_parent; }
	Node*       getChild(int _i) const             { APT_ASSERT(_i < (int)m_children.size()); return m_children[_i]; }
	int         getChildCount() const              { return (int)m_children.size(); }

	void        setStateMask(uint8 _mask)          { m_stateMask = _mask; }
	void        setActive(bool _state)             { m_stateMask = _state ? (m_stateMask | (uint8)kStateActive)   : (m_stateMask & ~(uint8)kStateActive); }
	void        setSelected(bool _state)           { m_stateMask = _state ? (m_stateMask | (uint8)kStateSelected) : (m_stateMask & ~(uint8)kStateSelected); }
	void        setDynamic(bool _state)            { m_stateMask = _state ? (m_stateMask | (uint8)kStateDynamic)  : (m_stateMask & ~(uint8)kStateDynamic); }
	bool        isActive() const                   { return (getStateMask() & (uint8)kStateActive)   != 0; }
	bool        isSelected() const                 { return (getStateMask() & (uint8)kStateSelected) != 0; }
	bool        isDynamic() const                  { return (getStateMask() & (uint8)kStateDynamic) != 0; }

private:
	mat4                m_localMatrix; //< Constructed from the xform stack.
	mat4                m_worldMatrix; //< Constructed from the scene hierarchy.
	
	uint8               m_type;        //< See NodeType.
	uint8               m_stateMask;   //< Combination of State 
	const void*         m_userData;    //< Render info, etc.

	Node*               m_parent;
	std::vector<Node*>  m_children;
	std::vector<XForm*> m_xforms;      //< Evaluated in reverse order, applied to m_localMatrix.

	NameStr             m_name;        //< Names may not be unique.
	Id                  m_id;          //< IDs must be unique per node.
	

	Node(Id _id, const char* _name = 0, Type _type = kTypeRoot, uint8 _stateMask = 0, const void* _userData = 0);

	/// Maintains traversability by reparenting child nodes to m_parent.	
	~Node();

	/// Move _xform within the stack; _dir is an offset from the current index.
	void moveXForm(const XForm* _xform, int _dir);

	/// Auto name based on type, e.g. Camera_001, Drawable_123
	static void AutoName(Type _type, NameStr& out_);

}; // class Node

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
	/// \note Names are not guranteed to be unique, however you can build a list
	///   of all matching nodes by repeatedly calling findNode() and passing the
	///   previous result to the _start parm until the function returns 0. Very
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

		bool     m_showNodeGraph3d; //< Show graph in 3d viewport.
		bool     m_showCameras;     //< Show camera frusta.

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

#endif // frm_Scene_h
