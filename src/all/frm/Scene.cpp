#include <frm/Scene.h>

#include <frm/Camera.h>
#include <frm/Profiler.h>
#include <frm/XForm.h>

#include <algorithm> // std::find

using namespace frm;
using namespace apt;

/*******************************************************************************

                                   Node

*******************************************************************************/

static const char* kNodeTypeStr[] =
{
	"Root",
	"Camera",
	"Object",
	"Light"
};

// PUBLIC

void Node::setNamef(const char* _fmt, ...)
{
	va_list args;
	va_start(args, _fmt);
	m_name.setfv(_fmt, args);
	va_end(args);
}

void Node::setParent(Node* _node)
{
	APT_ASSERT(_node);
	_node->addChild(this); // addChild sets m_parent implicitly
}

void Node::addChild(Node* _node)
{
	APT_ASSERT(_node);
	APT_ASSERT(std::find(m_children.begin(), m_children.end(), _node) == m_children.end()); // added the same child multiple times?
	m_children.push_back(_node);
	if (_node->m_parent && _node->m_parent != this) {
		_node->m_parent->removeChild(_node);
	}
	_node->m_parent = this;
}

void Node::removeChild(Node* _node)
{
	APT_ASSERT(_node);
	auto it = std::find(m_children.begin(), m_children.end(), _node);
	if (it != m_children.end()) {
		(*it)->m_parent = nullptr;
		m_children.erase(it);
	}
}


// PRIVATE

Node::Node(Id _id)
	: m_id(_id)
	, m_type(kTypeCount)
	, m_state(0x00)
	, m_userData(0)
	, m_sceneData(0)
	, m_localMatrix(mat4(1.0f))
	, m_parent(0)
{
}

Node::~Node()
{
 // re-parent children
	for (int i = 0, n = (int)m_children.size(); i < n; ++i) {
		m_children[i]->setParent(m_parent);
	}
 // de-parent this
	if (m_parent) {
		m_parent->removeChild(this);
	}
}

int Node::moveXForm(int _i, int _dir)
{
	int j = APT_CLAMP(_i + _dir, 0, (int)(m_xforms.size() - 1));
	std::swap(m_xforms[_i], m_xforms[j]);
	return j;
}


/*******************************************************************************

                                   Scene

*******************************************************************************/
static Scene s_defaultScene;
static unsigned s_typeCounters[Node::kTypeCount] = {}; // for auto name
Scene* Scene::s_currentScene = &s_defaultScene;

// PUBLIC

Scene::Scene()
	: m_root(s_nextNodeId++)
	, m_nodePool(128)
	, m_cameraPool(16)
	, m_drawCamera(nullptr)
	, m_cullCamera(nullptr)
{
	m_root.setName("ROOT");
	m_root.setType(Node::kTypeRoot);
	m_root.setStateMask(Node::kStateAny);
	m_root.setSceneDataScene(this);
}

Scene::~Scene()
{
	while (!m_cameras.empty()) {
		m_cameraPool.free(m_cameras.back());
		m_cameras.pop_back();
	}
	for (int i = 0; i < Node::kTypeCount; ++i) {
		while (!m_nodes[i].empty()) {
			m_nodePool.free(m_nodes[i].back());
			m_nodes[i].pop_back();
		}
	}
}

void Scene::update(float _dt, uint8 _stateMask)
{
	CPU_AUTO_MARKER("Scene::update");
	
	update(&m_root, _dt, _stateMask);
}

bool Scene::traverse(OnVisit* _callback, Node* _root_, uint8 _stateMask)
{
	CPU_AUTO_MARKER("Scene::traverse");

	if (_root_->getStateMask() & _stateMask) {
		if (!_callback(_root_)) {
			return false;
		}
		for (int i = 0; i < _root_->getChildCount(); ++i) {
			if (!traverse(_callback, _root_->getChild(i), _stateMask)) {
				return false;
			}
		}
	}
	return true;
}

Node* Scene::createNode(Node::Type _type, Node* _parent)
{
	CPU_AUTO_MARKER("Scene::createNode");

	Node* ret = m_nodePool.alloc(Node(s_nextNodeId++));
	AutoName(_type, ret->m_name);
	ret->m_type = _type;
	_parent = _parent ? _parent : &m_root;
	_parent->addChild(ret);
	m_nodes[_type].push_back(ret);
	++s_typeCounters[_type]; // auto name counter
	return ret;
}

void Scene::destroyNode(Node*& _node_)
{
	CPU_AUTO_MARKER("Scene::destroyNode");

	APT_ASSERT(_node_ != &m_root); // can't destroy the root
	
	Node::Type type = _node_->getType();
	switch (type) {
		case Node::kTypeCamera:
			if (_node_->m_sceneData) {
				Camera* camera = _node_->getSceneDataCamera();
				auto it = std::find(m_cameras.begin(), m_cameras.end(), camera);
				if (it != m_cameras.end()) {
					APT_ASSERT(camera->getNode() == _node_); // _node_ points to camera, but camera doesn't point to _node_
					m_cameras.erase(it);
				}
				m_cameraPool.free(camera);
			}
			break;
		default:
			break;
	};

	auto it = std::find(m_nodes[type].begin(), m_nodes[type].end(), _node_);
	if (it != m_nodes[type].end()) {
		m_nodes[type].erase(it);
		m_nodePool.free(_node_);
		_node_ = 0;
	}
}

Node* Scene::findNode(Node::Id _id, Node::Type _typeHint)
{
	CPU_AUTO_MARKER("Scene::findNode");

	Node* ret = nullptr;
	if (_typeHint != Node::kTypeCount) {
		for (auto it = m_nodes[_typeHint].begin(); it != m_nodes[_typeHint].end(); ++it) {
			if ((*it)->getId() == _id) {
				ret = *it;
				break;
			}
		}
	}
	for (int i = 0; ret == nullptr && i < Node::kTypeCount; ++i) {
		if (i == _typeHint) {
			continue;
		}
		for (auto it = m_nodes[_typeHint].begin(); it != m_nodes[_typeHint].end(); ++it) {
			if ((*it)->getId() == _id) {
				ret = *it;
				break;
			}
		}
	}
	return ret;
}

Camera* Scene::createCamera(const Camera& _copyFrom, Node* _parent_)
{
	CPU_AUTO_MARKER("Scene::createCamera");

	Camera* ret = m_cameraPool.alloc(_copyFrom);
	Node* node = createNode(Node::kTypeCamera, _parent_);
	node->setSceneDataCamera(ret);
	ret->setNode(node);

	m_cameras.push_back(ret);
	if (!m_drawCamera) {
		m_drawCamera = ret;
		m_cullCamera = ret;
	}
	return ret;
}

void Scene::destroyCamera(Camera*& _camera_)
{
	CPU_AUTO_MARKER("Scene::destroyCamera");

	Node* node = _camera_->getNode();
	APT_ASSERT(node);
	destroyNode(node); // implicitly destroys camera
	if (m_drawCamera == _camera_) {
		m_drawCamera = nullptr;
	}
	if (m_cullCamera == _camera_) {
		m_cullCamera = nullptr;
	}
	_camera_ = nullptr;
}

// PRIVATE

Node::Id Scene::s_nextNodeId = 0;

void Scene::update(Node* _node_, float _dt, uint8 _stateMask)
{
	if (!(_node_->m_state & _stateMask)) {
		return;
	}

 // reset world matrix
	_node_->m_worldMatrix = _node_->m_localMatrix;

 // apply xforms
	for (auto it = _node_->m_xforms.begin(); it != _node_->m_xforms.end(); ++it) {
		(*it)->apply(_node_, _dt);
	}

 // move to parent space
	if (_node_->m_parent) {
		_node_->m_worldMatrix = _node_->m_parent->m_worldMatrix * _node_->m_worldMatrix;
	}

 // type-specific update
	switch (_node_->getType()) {
		case Node::kTypeCamera: {
		 // copy world matrix into the camera
			Camera* camera = _node_->getSceneDataCamera();
			APT_ASSERT(camera);
			APT_ASSERT(camera->getNode() == _node_);
			camera->build();
			}
			break;
		default: 
			break;
	};

 // update children
	for (auto it = _node_->m_children.begin(); it != _node_->m_children.end(); ++it) {
		update(*it, _dt, _stateMask);
	}
}

void Scene::AutoName(Node::Type _type, Node::NameStr& out_)
{
	out_.setf("%s_%03u", kNodeTypeStr[_type], s_typeCounters[_type]);
}
