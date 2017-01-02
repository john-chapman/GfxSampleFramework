#include <frm/Scene.h>

#include <frm/Camera.h>
#include <frm/Profiler.h>
#include <frm/XForm.h>

#include <apt/log.h>
#include <apt/Json.h>

#include <algorithm> // std::find
#include <utility> // std::swap

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
static Node::Type NodeTypeFromStr(const char* _str)
{
	for (int i = 0; i < Node::kTypeCount; ++i) {
		if (strcmp(kNodeTypeStr[i], _str) == 0) {
			return (Node::Type)i;
		}
	}
	return Node::kTypeCount;
}

// PUBLIC

void Node::setNamef(const char* _fmt, ...)
{
	va_list args;
	va_start(args, _fmt);
	m_name.setfv(_fmt, args);
	va_end(args);
}

void Node::addXForm(XForm* _xform)
{
	APT_ASSERT(_xform);
	APT_ASSERT(_xform->getNode() == nullptr);
	_xform->setNode(this);
	m_xforms.push_back(_xform);
}

void Node::removeXForm(XForm* _xform)
{
	for (auto it = m_xforms.begin(); it != m_xforms.end(); ++it) {
		XForm* x = *it;
		if (x == _xform) {
			APT_ASSERT(x->getNode() == this);
			x->setNode(nullptr);
			m_xforms.erase(it);
			return;
		}
	}
}

void Node::setParent(Node* _node)
{
	if (_node) {
		_node->addChild(this); // addChild sets m_parent implicitly
	} else {
		if (m_parent) {
			m_parent->removeChild(this);
		}
		m_parent = nullptr;
	}
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
	for (auto it = m_children.begin(); it != m_children.end(); ++it) {
		Node* n = *it;
		n->m_parent = nullptr; // prevent m_parent->addChild calling removeChild on this (invalidates it)
		if (m_parent) {
			m_parent->addChild(n->m_parent);
		}
	}
 // de-parent this
	if (m_parent) {
		m_parent->removeChild(this);
	}

 // delete xforms
	for (auto it = m_xforms.begin(); it != m_xforms.end(); ++it) {
		delete *it;
	}
	m_xforms.clear();
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

void frm::swap(Scene& _a, Scene& _b)
{
	std::swap(_a.m_nextNodeId, _b.m_nextNodeId);
	std::swap(_a.m_root,       _b.m_root);
	std::swap(_a.m_nodes,      _b.m_nodes);
	apt::swap(_a.m_nodePool,   _b.m_nodePool);
	std::swap(_a.m_drawCamera, _b.m_drawCamera);
	std::swap(_a.m_cullCamera, _b.m_cullCamera);
	std::swap(_a.m_cameras,    _b.m_cameras);
	apt::swap(_a.m_cameraPool, _b.m_cameraPool);
}


// PUBLIC

bool Scene::Load(const char* _path, Scene& scene_)
{
	APT_LOG("Loading scene from '%s'", _path);
	Json json;
	if (!Json::Read(json, _path)) {
		return false;
	}
	JsonSerializer serializer(&json, JsonSerializer::kRead);
	Scene newScene;
	
	if (!newScene.serialize(serializer)) {
		return false;
	}
	swap(newScene, scene_);
	return true;
}

bool Scene::Save(const char* _path, Scene& _scene)
{
	APT_LOG("Saving scene to '%s'", _path);
	Json json;
	JsonSerializer serializer(&json, JsonSerializer::kWrite);
	if (!_scene.serialize(serializer)) {
		return false;
	}
	return Json::Write(json, _path);
}

Scene::Scene()
	: m_nextNodeId(0)
	, m_nodePool(128)
	, m_cameraPool(16)
	, m_drawCamera(nullptr)
	, m_cullCamera(nullptr)
#ifdef frm_Scene_ENABLE_EDIT
	, m_showNodeGraph3d(false)
	, m_editNode(nullptr)
	, m_editXForm(nullptr)
	, m_editCamera(nullptr)
#endif
{
	m_root = m_nodePool.alloc(Node(m_nextNodeId++));
	m_nodes[Node::kTypeRoot].push_back(m_root);
	m_root->setName("ROOT");
	m_root->setType(Node::kTypeRoot);
	m_root->setStateMask(Node::kStateAny);
	m_root->setSceneDataScene(this);
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
	
	update(m_root, _dt, _stateMask);
}

bool Scene::traverse(Node* _root_, uint8 _stateMask, OnVisit* _callback)
{
	CPU_AUTO_MARKER("Scene::traverse");

	if (_root_->getStateMask() & _stateMask) {
		if (!_callback(_root_)) {
			return false;
		}
		for (int i = 0; i < _root_->getChildCount(); ++i) {
			if (!traverse(_root_->getChild(i), _stateMask, _callback)) {
				return false;
			}
		}
	}
	return true;
}

Node* Scene::createNode(Node::Type _type, Node* _parent)
{
	CPU_AUTO_MARKER("Scene::createNode");

	Node* ret = m_nodePool.alloc(Node(m_nextNodeId++));
	AutoName(_type, ret->m_name);
	ret->m_type = _type;
	_parent = _parent ? _parent : m_root;
	_parent->addChild(ret);
	m_nodes[_type].push_back(ret);
	++s_typeCounters[_type]; // auto name counter
	return ret;
}

void Scene::destroyNode(Node*& _node_)
{
	CPU_AUTO_MARKER("Scene::destroyNode");

	APT_ASSERT(_node_ != m_root); // can't destroy the root
	
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
		_node_ = nullptr;
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
		for (auto it = m_nodes[i].begin(); it != m_nodes[i].end(); ++it) {
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

bool Scene::serialize(JsonSerializer& _serializer_)
{
	if (_serializer_.getMode() == JsonSerializer::kRead) {
		if (!serialize(_serializer_, *m_root)) {
			APT_LOG_ERR("Scene::serialize: Read error");
			return false;
		}

	#ifdef frm_Scene_ENABLE_EDIT
		m_editNode   = nullptr;
		m_editXForm  = nullptr;
		m_editCamera = nullptr;
	#endif

	} else {
		if (!serialize(_serializer_, *m_root)) {
			return false;
		}
	}
	 
	Node::Id drawCameraId = Node::kInvalidId;
	Node::Id cullCameraId = Node::kInvalidId;
	if (_serializer_.getMode() == JsonSerializer::kWrite) {
		if (m_drawCamera != nullptr && m_drawCamera->getNode() != nullptr) {
			drawCameraId = m_drawCamera->getNode()->getId();	
		}
		if (m_cullCamera != nullptr && m_cullCamera->getNode() != nullptr) {
			cullCameraId = m_cullCamera->getNode()->getId();
		}
	}
	_serializer_.value("DrawCameraId", drawCameraId);
	_serializer_.value("CullCameraId", cullCameraId);
	if (_serializer_.getMode() == JsonSerializer::kRead) {
		if (drawCameraId != Node::kInvalidId) {
			Node* n = findNode(drawCameraId, Node::kTypeCamera);
			if (n != nullptr) {
				m_drawCamera = n->getSceneDataCamera();
			}
		}
		if (cullCameraId != Node::kInvalidId) {
			Node* n = findNode(cullCameraId, Node::kTypeCamera);
			if (n != nullptr) {
				m_cullCamera = n->getSceneDataCamera();
			}
		}
	}

	return true;
}

bool Scene::serialize(JsonSerializer& _serializer_, Node& _node_)
{
	_serializer_.value("Id",          _node_.m_id);
	_serializer_.value("Name",        (StringBase&)_node_.m_name);
	_serializer_.value("State",       _node_.m_state);
	_serializer_.value("UserData",    _node_.m_userData);
	_serializer_.value("LocalMatrix", _node_.m_localMatrix);

	String<64> tmp = kNodeTypeStr[_node_.m_type];
	_serializer_.value("Type", (StringBase&)tmp);
	if (_serializer_.getMode() == JsonSerializer::kRead) {
		_node_.m_type = NodeTypeFromStr(tmp);
		if (_node_.m_type == Node::kTypeCount) {
			APT_LOG_ERR("Scene: Invalid node type '%s'", (const char*)tmp);
			return false;
		}

		switch (_node_.m_type) {
			case Node::kTypeRoot: {
				_node_.setSceneDataScene(this);
				break;
			}
			case Node::kTypeCamera: {
				Camera* cam = m_cameraPool.alloc();
				cam->setNode(&_node_);
				if (!cam->serialize(_serializer_)) {
					m_cameraPool.free(cam);
					return false;
				}
				m_cameras.push_back(cam);
				_node_.setSceneDataCamera(cam);
				break;
			}
			default:
				break;
		};
		m_nextNodeId = APT_MAX(m_nextNodeId, _node_.m_id + 1);

		if (_serializer_.beginArray("Children")) {
			while (_serializer_.beginObject()) {
				Node* child = m_nodePool.alloc(Node(m_nextNodeId)); // node id gets overwritten in the next call to serialize()
				if (!serialize(_serializer_, *child)) {
					m_nodePool.free(child);
					return false;
				}
				child->m_parent = &_node_;
				_node_.m_children.push_back(child);
				m_nodes[child->m_type].push_back(child);
				_serializer_.endObject();
			}
			_serializer_.endArray();
		}

		if (_serializer_.beginArray("XForms")) {
			while (_serializer_.beginObject()) {
				_serializer_.value("Class", (StringBase&)tmp);
				XForm* xform = XForm::Create(StringHash(tmp));
				if (xform) {
					xform->serialize(_serializer_);
					xform->setNode(&_node_);
					_node_.m_xforms.push_back(xform);
				} else {
					APT_LOG_ERR("Scene: Invalid xform '%s'", (const char*)tmp);
				}
				_serializer_.endObject();
			}
			_serializer_.endArray();
		}
		
	} else { // if writing
		switch (_node_.m_type) {
			case Node::kTypeCamera: {
				Camera* cam = _node_.getSceneDataCamera();
				if (!cam->serialize(_serializer_)) {
					return false;
				}
				break;
			}
			default:
				break;
		};

		if (!_node_.m_children.empty()) {
			_serializer_.beginArray("Children");
				for (auto& child : _node_.m_children) {
					if (child->getName()[0] == '#') {
						continue;
					}
					_serializer_.beginObject();
						serialize(_serializer_, *child);
					_serializer_.endObject();
				}
			_serializer_.endArray();
		}

		if (!_node_.m_xforms.empty()) {
			_serializer_.beginArray("XForms");
				for (auto& xform : _node_.m_xforms) {
					_serializer_.beginObject();
						const char* className = xform->getClassRef()->getName();
						_serializer_.string("Class", const_cast<char*>(className));
						xform->serialize(_serializer_);
					_serializer_.endObject();
				}
			_serializer_.endArray();
		}
	}

	return true;
}

#ifdef frm_Scene_ENABLE_EDIT

#include <frm/Im3d.h>
#include <imgui/imgui.h>

static const Im3d::Color kNodeTypeCol[] = 
{
	Im3d::Color(0.5f, 0.5f, 0.5f, 0.5f), // kTypeRoot,
	Im3d::Color(0.5f, 0.5f, 1.0f, 0.5f), // kTypeCamera,
	Im3d::Color(0.5f, 1.0f, 0.5f, 1.0f), // kTypeObject,
	Im3d::Color(1.0f, 0.5f, 0.0f, 1.0f)  // kTypeLight,
};

void Scene::edit()
{
	ImGui::Begin("Scene", 0, 
		ImGuiWindowFlags_NoResize | 
		ImGuiWindowFlags_AlwaysAutoResize
		);

	if (ImGui::TreeNode("Node Counters")) {
		int totalNodes = 0;
		for (int i = 0; i < Node::kTypeCount; ++i) {
			ImGui::Text("%-15s: %d", kNodeTypeStr[i], (int)m_nodes[i].size());
			totalNodes += (int)m_nodes[i].size();
		}
		ImGui::Spacing();
		ImGui::Text("Total          : %d", totalNodes);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Hierarchy")) {
		drawHierarchy(m_root);
		ImGui::TreePop();
	}

	ImGui::Checkbox("Show Node Graph", &m_showNodeGraph3d);
	if (m_showNodeGraph3d) {
		Im3d::PushDrawState();
		Im3d::PushMatrix();
		Im3d::SetAlpha(1.0f);
		traverse(
			m_root, Node::kStateAny, 
			[](Node* _node_)->bool {
				Im3d::SetMatrix(_node_->getWorldMatrix());
				Im3d::DrawXyzAxes();
				Im3d::BeginPoints();
					Im3d::Vertex(Im3d::Vec3(0.0f), 4.0f, kNodeTypeCol[_node_->getType()]);
				Im3d::End();
				Im3d::SetIdentity();
				if (_node_->getParent() && _node_->getParent() != Scene::GetCurrent().getRoot()) {
					Im3d::SetColor(1.0f, 0.0f, 1.0f);
					Im3d::BeginLines();
						Im3d::SetAlpha(0.25f);
						Im3d::Vertex(GetTranslation(_node_->getWorldMatrix()));
						Im3d::SetAlpha(1.0f);
						Im3d::Vertex(GetTranslation(_node_->getParent()->getWorldMatrix()));
					Im3d::End();
				}
				return true;
			});
		Im3d::PopMatrix();
		Im3d::PopDrawState();
	}

	ImGui::Spacing();
	editNodes();
	
	ImGui::Spacing();
	editCameras();

	ImGui::End(); // Scene
}

void Scene::editNodes()
{
	if (ImGui::CollapsingHeader("Nodes")) {
		ImGui::PushID("SelectNode");
			if (ImGui::Button("Select")) {
				beginSelectNode();
			}
			Node* newEditNode = selectNode(m_editNode);
		ImGui::PopID();

		ImGui::SameLine();
		if (ImGui::Button("Create")) {
			beginCreateNode();
		}
		newEditNode = createNode(newEditNode);

		if (m_editNode) {
			bool destroy = false;

			ImGui::SameLine();
			if (ImGui::Button("Destroy")) {
				destroy = true;
			 // don't destroy the last root/camera
			 // \todo modal warning when deleting a root or a node with children?
				if (m_editNode->getType() == Node::kTypeRoot || m_editNode->getType() == Node::kTypeCamera) {
					if (m_nodes[m_editNode->getType()].size() == 1) {
						APT_LOG_ERR("Error: Can't delete the only %s", kNodeTypeStr[m_editNode->getType()]);
						destroy = false;
					}
				}
			}

			ImGui::Separator();
			ImGui::Spacing();

		 // local matrix
			if (Im3d::Gizmo("EditNodeGizmo", &m_editNode->m_localMatrix)) {
				update(m_editNode, 0.0f, Node::kStateAny); // force node update
			}

		 // name
			static Node::NameStr s_nameBuf;
			s_nameBuf.set(m_editNode->m_name);
			if (ImGui::InputText("Name", s_nameBuf, s_nameBuf.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)) {
				m_editNode->m_name.set(s_nameBuf);
			}

		 // state
			bool active = m_editNode->isActive();
			bool dynamic = m_editNode->isDynamic();
			if (ImGui::Checkbox("Active", &active)) {
				m_editNode->setActive(active);
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Dynamic", &dynamic)) {
				m_editNode->setDynamic(dynamic);
			}

		 // parent
		 // \todo check for loops
			ImGui::Spacing();
			ImGui::PushID("SelectParent");
				if (ImGui::Button("Parent ->")) {
					beginSelectNode();
				}
				Node* newParent = selectNode(m_editNode->getParent());
				if (newParent == m_editNode) {
					APT_LOG_ERR("Error: Can't parent a node to itself");
					newParent = m_editNode->getParent();
				}
			ImGui::PopID();

			if (newParent != m_editNode->getParent()) {
				m_editNode->setParent(newParent);
			 // \todo modify local matrix so that the node position doesn't change
				m_editNode->m_localMatrix = m_editNode->m_worldMatrix * inverse(newParent->getWorldMatrix());
			}
			ImGui::SameLine();
			if (m_editNode->getParent()) {
				ImGui::Text(m_editNode->getParent()->getName());
				if (ImGui::IsItemClicked()) {
					newEditNode = m_editNode->getParent();
				}
			} else {
				ImGui::Text("--");
			}

		 // children
			if (!m_editNode->m_children.empty()) {
				ImGui::Spacing();
				if (ImGui::TreeNode("Children")) {
					for (auto it = m_editNode->m_children.begin(); it != m_editNode->m_children.end(); ++it) {
						Node* child = *it;
						ImGui::Text("[%s] %s", kNodeTypeStr[child->getType()], child->getName());
						if (ImGui::IsItemClicked()) {
							newEditNode = child;
							break;
						}
					}

					ImGui::TreePop();
				}
			}

		 // xforms
			if (ImGui::TreeNode("XForms")) {

				if (ImGui::Button("Create")) {
					beginCreateXForm();
				}
				XForm* newEditXForm = createXForm(m_editXForm);

				if (!m_editNode->m_xforms.empty()) {
				 // build list for xform stack
					const char* xformList[64];
					APT_ASSERT(m_editNode->m_xforms.size() <= 64);
					int selectedXForm = 0;
					for (int i = 0; i < (int)m_editNode->m_xforms.size(); ++i) {
						XForm* xform = m_editNode->m_xforms[i];
						if (xform == m_editXForm) {
							selectedXForm = i;
						}
						xformList[i] = xform->getName();
					}
					ImGui::Spacing();
					if (ImGui::ListBox("##XForms", &selectedXForm, xformList, (int)m_editNode->m_xforms.size())) {
						m_editXForm = m_editNode->m_xforms[selectedXForm];
						newEditXForm = m_editXForm;
					}

					/*if (m_editXForm) {
						if (ImGui::Button("Move Up")) {
							m_editNode->moveXForm(m_editXForm, -1);
						}
						ImGui::SameLine();
						if (ImGui::Button("MoveDown")) {
							m_editNode->moveXForm(m_editXForm, 1);
						}
					}*/

					if (m_editXForm) {
						ImGui::Separator();
						ImGui::Spacing();
						ImGui::PushID(m_editXForm);
							m_editXForm->edit();
						ImGui::PopID();
					}

				}

				if (m_editXForm != newEditXForm) {
					m_editNode->addXForm(newEditXForm);
					m_editXForm = newEditXForm;
				}

				ImGui::TreePop();
			}

		 // deferred destroy
			if (destroy) {
				if (m_editNode->getType() == Node::kTypeCamera) {
				 // destroyNode will implicitly destroy camera, so deselect camera if selected
					if (m_editNode->getSceneDataCamera() == m_editCamera) {
						m_editCamera = nullptr;
					}
				}
				destroyNode(m_editNode);
				newEditNode = nullptr; // prevent dangling ptr
			}
		}
	 // deferred select
		if (newEditNode != m_editNode) {
			// modify selection (\todo 1 selection per node type?)
			if (m_editNode && m_editNode->getType() == newEditNode->getType()) {
				m_editNode->setSelected(false);
			}
			if (newEditNode) {
				newEditNode->setSelected(true);
			}
			m_editNode = newEditNode;
			m_editXForm = nullptr;
		}
	}
}

void Scene::editCameras()
{
	if (ImGui::CollapsingHeader("Cameras")) {
		ImGui::PushID("SelectCamera");
			if (ImGui::Button("Select##Camera")) {
				beginSelectCamera();
			}
			Camera* newEditCamera = selectCamera(m_editCamera);
		ImGui::PopID();

		ImGui::SameLine();
		if (ImGui::Button("Create")) {
			newEditCamera = createCamera(Camera());
		}

		if (m_editCamera) {
			bool destroy = false;

			ImGui::SameLine();
			if (ImGui::Button("Destroy")) {
				destroy = true;
				if (m_cameras.size() == 1) {
					APT_LOG_ERR("Error: Can't delete the only Camera");
					destroy = false;
				}
			}

			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Set Draw Camera")) {
				m_drawCamera = m_editCamera;
			}
			ImGui::SameLine();
			if (ImGui::Button("Set Cull Camera")) {
				m_cullCamera = m_editCamera;
			}
			if (ImGui::Button("Set Current Node")) {
			 // deselect any camera nodes
				for (int i = 0; i < getNodeCount(Node::kTypeCamera); ++i) {
					Node* node = getNode(Node::kTypeCamera, i);
					if (node->isSelected()) {
						node->setSelected(false);
					}
				}
				m_editCamera->getNode()->setSelected(true);
			}

			ImGui::Spacing();
			static Node::NameStr s_nameBuf;
			s_nameBuf.set(m_editCamera->getNode()->m_name);
			if (ImGui::InputText("Name", s_nameBuf, s_nameBuf.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)) {
				m_editCamera->getNode()->m_name.set(s_nameBuf);
			}

			bool  ortho     = m_editCamera->isOrtho();
			bool  symmetric = m_editCamera->isSymmetric();
			float up        = degrees(atan(m_editCamera->getTanFovUp()));
			float down      = degrees(atan(m_editCamera->getTanFovDown()));
			float left      = degrees(atan(m_editCamera->getTanFovLeft()));
			float right     = degrees(atan(m_editCamera->getTanFovRight()));
			float clipNear  = m_editCamera->getClipNear();
			float clipFar   = m_editCamera->getClipFar();

			if (ImGui::Checkbox("Orthographic", &ortho)) {
				m_editCamera->setIsOrtho(ortho);
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Symmetrical", &symmetric)) {
				m_editCamera->setIsSymmetric(symmetric);
			}
			if (ortho) {
				if (ImGui::SliderFloat("Top", &up, 0.0f, 100.0f)) {
					m_editCamera->setFovUp(radians(up));
				}
				if (ImGui::SliderFloat("Bottom", &down, 0.0f, 100.0f)) {
					m_editCamera->setFovDown(radians(down));
				}
				if (ImGui::SliderFloat("Left", &left, 0.0f, 100.0f)) {
					m_editCamera->setFovLeft(radians(left));
				}
				if (ImGui::SliderFloat("Right", &right, 0.0f, 100.0f)) {
					m_editCamera->setFovRight(radians(right));
				}
			} else {
				if (symmetric) {
					float fov = up + down;
					float aspect = m_editCamera->getAspect();
					if (ImGui::SliderFloat("Fov Vertical", &fov, 0.0f, 180.0f)) {
						m_editCamera->setVerticalFov(radians(fov));
					}
					if (ImGui::SliderFloat("Aspect Ratio", &aspect, 0.0f, 2.0f)) {
						m_editCamera->setAspect(aspect);
					}
				} else {
					if (ImGui::SliderFloat("Fov Up", &up, 0.0f, 90.0f)) {
						m_editCamera->setFovUp(radians(up));
					}
					if (ImGui::SliderFloat("Fov Down", &down, 0.0f, 90.0f)) {
						m_editCamera->setFovDown(radians(down));
					}
					if (ImGui::SliderFloat("Fov Left", &left, 0.0f, 90.0f)) {
						m_editCamera->setFovLeft(radians(left));
					}
					if (ImGui::SliderFloat("Fov Right", &right, 0.0f, 90.0f)) {
						m_editCamera->setFovRight(radians(right));
					}
				}
			}
			if (ImGui::SliderFloat("Clip Near", &clipNear, 0.0f, 2.0f)) {
				m_editCamera->setClipNear(clipNear);
			}
			if (ImGui::SliderFloat("Clip Far", &clipFar, clipNear, 5000.0f)) {
				m_editCamera->setClipFar(clipFar);
			}

		 // deferred destroy
			if (destroy) {
				if (m_editNode == m_editCamera->getNode()) {
					m_editNode = nullptr;
				}
				destroyCamera(m_editCamera);
				newEditCamera = m_cameras[0];
			}
		}
	 // deferred select
		m_editCamera = newEditCamera;
		
	}
}

void Scene::beginSelectNode()
{
	ImGui::OpenPopup("Select Node");
}
Node* Scene::selectNode(Node* _current, Node::Type _type)
{
	Node* ret = _current;

 // popup selection
	if (ImGui::BeginPopup("Select Node")) {
		static ImGuiTextFilter filter;
		filter.Draw("Filter##Node");
		int type = _type == Node::kTypeCount ? 0 : _type;
		int typeEnd = APT_MIN((int)_type + 1, (int)Node::kTypeCount);
		for (; type < typeEnd; ++type) {
			for (int node = 0; node < (int)m_nodes[type].size(); ++node) {
				if (m_nodes[type][node] == _current) {
					continue;
				}
				String<32> tmp("[%s] %s", kNodeTypeStr[type], m_nodes[type][node]->getName());
				if (filter.PassFilter(tmp)) {
					if (ImGui::Selectable(tmp)) {
						ret = m_nodes[type][node];
						break;
					}
				}
			}
		}
		ImGui::EndPopup();
	}

 // 3d selection
	// \todo 

	return ret;
}

void Scene::beginSelectCamera()
{
	ImGui::OpenPopup("Select Camera");
}
Camera* Scene::selectCamera(Camera* _current)
{
	Camera* ret = _current;
	if (ImGui::BeginPopup("Select Camera")) {
		static ImGuiTextFilter filter;
		filter.Draw("Filter##Camera");
		for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it) {
			Camera* cam = *it;
			if (cam == _current) {
				continue;
			}
			APT_ASSERT(cam->getNode());
			if (filter.PassFilter(cam->getNode()->getName())) {
				if (ImGui::Selectable(cam->getNode()->getName())) {
					ret = cam;
					break;
				}
			}
		}
		
		ImGui::EndPopup();
	}
	return ret;
}

void Scene::beginCreateNode()
{
	ImGui::OpenPopup("Create Node");
}
Node* Scene::createNode(Node* _current)
{
	Node* ret = _current;
	if (ImGui::BeginPopup("Create Node")) {
		static const char* kNodeTypeComboStr = "Root\0Camera\0Object\0Light\0";
		static int s_type = Node::kTypeObject;
		ImGui::Combo("Type", &s_type , kNodeTypeComboStr);

		static Node::NameStr s_name;
		ImGui::InputText("Name", s_name, s_name.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue);
		AutoName((Node::Type)s_type, s_name);

		if (ImGui::Button("Create")) {
			ret = createNode((Node::Type)s_type);
			ret->setName(s_name);
			ret->setStateMask(Node::kStateActive | Node::kStateDynamic | Node::kStateSelected);
			switch (ret->getType()) {
				case Node::kTypeRoot:   ret->setSceneDataScene(this); break;
				case Node::kTypeCamera: APT_ASSERT(false); break; // \todo creata a camera in this case?
				case Node::kTypeLight:  APT_ASSERT(false); break; // \todo creata a light in this case?
				default:                break;
			};
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	return ret;
}

void Scene::drawHierarchy(Node* _node)
{
	String<32> tmp("[%s] %s %s", kNodeTypeStr[_node->getType()], _node->getName(), _node->isSelected() ? "*" : "");
	ImVec4 col = ImColor(0.1f, 0.1f, 0.1f, 1.0f); // = inactive
	if (_node->isActive()) {
		if (_node->isDynamic()) {
			col = ImColor(IM_COL32_GREEN); // = active, dynamic
		} else {
			col = ImColor(IM_COL32_YELLOW); // = active, static
		}
	}
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	if (_node->getChildCount() == 0) {
		ImGui::Text(tmp);
	} else {
		if (ImGui::TreeNode(tmp)) {
			for (int i = 0; i < _node->getChildCount(); ++i) {
				drawHierarchy(_node->getChild(i));
			}
			ImGui::TreePop();
		}
	}

	ImGui::PopStyleColor();
}

void Scene::beginCreateXForm()
{
	ImGui::OpenPopup("Create XForm");
}
XForm* Scene::createXForm(XForm* _current)
{
	XForm* ret = _current;
	if (ImGui::BeginPopup("Create XForm")) {
		static ImGuiTextFilter filter;
		filter.Draw("Filter##XForm");
		XForm::ClassRef* xformRef = 0;
		for (int i = 0; i < XForm::GetClassRefCount(); ++i) {
			const XForm::ClassRef* cref = XForm::GetClassRef(i);
			if (filter.PassFilter(cref->getName())) {
				if (ImGui::Selectable(cref->getName())) {
					ret = XForm::Create(cref);
					break;
				}
			}
		}
		ImGui::EndPopup();
	}
	return ret;
}

#endif // frm_Scene_ENABLE_EDIT

// PRIVATE

void Scene::update(Node* _node_, float _dt, uint8 _stateMask)
{
	if (!(_node_->m_state & _stateMask)) {
		return;
	}

 // reset world matrix
	_node_->m_worldMatrix = _node_->m_localMatrix;

 // apply xforms
	for (auto it = _node_->m_xforms.begin(); it != _node_->m_xforms.end(); ++it) {
		(*it)->apply(_dt);
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
