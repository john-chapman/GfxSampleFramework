#include <frm/Scene.h>

#include <frm/Camera.h>
#include <frm/Profiler.h>
#include <frm/XForm.h>

#include <apt/log.h>
#include <apt/String.h>

#include <algorithm> // std::find

using namespace frm;
using namespace apt;

static const char* kNodeTypeStr[] =
{
	"Root",
	"Camera",
	"Light",
	"Object"
};

/*******************************************************************************

                                   Node

*******************************************************************************/

// PUBLIC

void Node::applyXForms(float _dt)
{
	m_worldMatrix = m_localMatrix;
	for (int i = (int)m_xforms.size() - 1; i != -1; --i) {
		m_xforms[i]->apply(this, _dt);
	}
}

void Node::addXForm(XForm* _xform)
{
	m_xforms.push_back(_xform);
}

void Node::removeXForm(XForm* _xform)
{
	auto it = std::find(m_xforms.begin(), m_xforms.end(), _xform);
	APT_ASSERT(it != m_xforms.end()); // not found, removing an xform you didn't add?
	m_xforms.erase(it);
}

void Node::setParent(Node* _parent)
{
	APT_ASSERT(_parent);
	_parent->addChild(this); // add child sets m_parent implicitly
}

void Node::addChild(Node* _child)
{
	APT_ASSERT(std::find(m_children.begin(), m_children.end(), _child) == m_children.end()); // added the same child multiple times?
	m_children.push_back(_child);
	if (_child->m_parent && _child->m_parent != this) {
		_child->m_parent->removeChild(_child);
	}
	_child->m_parent = this;
}

void Node::removeChild(Node* _child)
{
	auto it = std::find(m_children.begin(), m_children.end(), _child);
	if (it != m_children.end()) {
		(*it)->m_parent = 0;
		m_children.erase(it);
	}
}

void Node::setNamef(const char* _fmt, ...)
{
	va_list args;
	va_start(args, _fmt);
	m_name.setfv(_fmt, args);
	va_end(args);
}


// PRIVATE

Node::Node(Id _id, const char* _name, Node::Type _type, uint8 _stateMask, const void* _userData)
	: m_localMatrix(mat4(1.0f))
	, m_type((uint8)_type)
	, m_stateMask(_stateMask)
	, m_userData(_userData)
	, m_parent(0)
	, m_name(_name)
	, m_id(_id)
{
	if (m_name.isEmpty()) {
		AutoName(_type, m_name);
	}
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

void Node::moveXForm(const XForm* _xform, int _dir)
{
	for (int i = 0; i < (int)m_xforms.size(); ++i) {
		if (m_xforms[i] == _xform) {
			int j = APT_CLAMP(i + _dir, 0, (int)(m_xforms.size() - 1));
			std::swap(m_xforms[i], m_xforms[j]);
			return;
		}
	}
}

static unsigned s_typeCounters[Node::kTypeCount] = {};
void Node::AutoName(Type _type, NameStr& out_)
{
	out_.setf("%s_%03u", kNodeTypeStr[_type], s_typeCounters[_type]);
}

/*******************************************************************************

                                   Scene

*******************************************************************************/

// PUBLIC

Scene::Scene()
	: m_root(s_nextNodeId++, "root", Node::kTypeRoot, Node::kStateActive | Node::kStateDynamic, this)
	, m_nodePool(128)
	, m_cameraPool(16)
{
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

bool Scene::traverse(VisitNode* _callback, Node* _root_, uint8 _stateMask)
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

Node* Scene::createNode(const char* _name, Node::Type _type, Node* _parent_, const void* _userData)
{
	CPU_AUTO_MARKER("Scene::createNode");

 // \todo active/dynamic by default? 
	Node* ret = m_nodePool.alloc(Node(s_nextNodeId++, _name, _type, Node::kStateActive | Node::kStateDynamic, _userData));
	++s_typeCounters[_type]; // auto name counter
	m_nodes[(int)_type].push_back(ret);
	_parent_ = _parent_ ? _parent_ : &m_root;
	_parent_->addChild(ret);
	return ret;
}


void Scene::destroyNode(Node*& _node_)
{
	CPU_AUTO_MARKER("Scene::destroyNode");

	APT_ASSERT(_node_ != &m_root); // don't destroy the root!
	
	Node::Type type = _node_->getType();

	switch (type) {
		case Node::kTypeCamera:
			if (_node_->m_userData) {
				Camera* camera = (Camera*)_node_->m_userData;
				for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it) {
					if (*it == camera) {
						APT_ASSERT(camera->getNode() == _node_); // _node_ points to camera, but camera doesn't point to _node_?
						m_cameras.erase(it);
						break;
					}
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

Node* Scene::findNode(const char* _name, const Node* _start)
{
	CPU_AUTO_MARKER("Scene::findNode");

	if (m_root.getName() == _name) {
		return &m_root;
	}
	
	for (int i = 0; i < (int)Node::kTypeCount; ++i) {
		auto it = m_nodes[i].begin();
		if (_start) {
			while (*it != _start && it != m_nodes[i].end()) {
				++it;
			}
			if (it != m_nodes[i].end()) {
				++it; // skip _start
			}
		}
		for ( ; it != m_nodes[i].end(); ++it) {
			if ((*it)->getName() == _name) {
				return *it;
			}
		}
	}
	return 0;
}

Node* Scene::findNode(Node::Id _id)
{
	CPU_AUTO_MARKER("Scene::findNode");

	if (m_root.getId() == _id) {
		return &m_root;
	}	
	for (int i = 0; i < (int)Node::kTypeCount; ++i) {
		for (auto it = m_nodes[i].begin(); it != m_nodes[i].end(); ++it) {
			if ((*it)->getId() == _id) {
				return *it;
			}
		}
	}
	return 0;
}


Camera* Scene::createCamera(const Camera& _copyFrom, Node* _parent_)
{
	Camera* ret;
	ret = m_cameraPool.alloc(_copyFrom);
	ret->setNode(createNode(0, Node::kTypeCamera, _parent_, (void*)ret));
	m_cameras.push_back(ret);
	if (!m_drawCamera) {
		m_drawCamera = ret;
		m_cullCamera = ret;
	}
	return ret;
}

void Scene::destroyCamera(Camera*& _camera_)
{
	Node* node = _camera_->getNode();
	if (node) {
		destroyNode(node); // implicitly destroys camera
	} else {
		for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it) {
			if (*it == _camera_) {
				m_cameraPool.free(_camera_);
				m_cameras.erase(it);
				break;
			}
		}
	}
	if (m_drawCamera == _camera_) {
		m_drawCamera = 0;
	}
	if (m_cullCamera == _camera_) {
		m_cullCamera = 0;
	}
	_camera_ = 0;
}


// PRIVATE

Node::Id Scene::s_nextNodeId = 0;

void Scene::update(Node* _node, float _dt, uint8 _stateMask)
{
	if (_node->m_stateMask & _stateMask) {
	 // construct the node matrices
		_node->applyXForms(_dt);
		if (_node->m_parent) {
			_node->m_worldMatrix = _node->m_parent->m_worldMatrix * _node->m_worldMatrix;
		}

	 // update children recursively
		for (int i = 0; i < _node->getChildCount(); ++i) {
			update(_node->getChild(i), _dt, _stateMask);
		}

	 // type-specific update
		switch (_node->getType()) {
			case Node::kTypeCamera:
			 // copy world matrix into the camera
				if (_node->m_userData) {
					Camera* camera = (Camera*)_node->m_userData;
					APT_ASSERT(camera->getNode() == _node);
					camera->build();
				}
				break;
			default: 
				break;
		};
	}
}


/*******************************************************************************

                              SceneEditor

*******************************************************************************/

#ifdef frm_Scene_ENABLE_EDIT

#include <frm/im3d.h>
#include <imgui/imgui.h>

static bool DrawNodeGraphCallback(Node* _node)
{
	static const vec3 kNodeGraphColor = vec3(1.0f, 0.0f, 0.5f);
	const Im3d::Vec3 pos = GetTranslation(_node->getWorldMatrix());
	Im3d::BeginPoints();			
		Im3d::Vertex(pos, 4.0f, Im3d::Color(kNodeGraphColor.r, kNodeGraphColor.g, kNodeGraphColor.b, 0.75f));
	Im3d::End();
	Im3d::SetSize(1.0f);
	Im3d::PushMatrix();
		Im3d::MulMatrix(_node->getWorldMatrix());
		Im3d::DrawXyzAxes();
	Im3d::PopMatrix();
	if (_node->getParent()) {
		const Im3d::Vec3 parentPos = GetTranslation(_node->getParent()->getWorldMatrix());
		Im3d::BeginLines();
			Im3d::Vertex(pos, Im3d::Color(kNodeGraphColor.r, kNodeGraphColor.g, kNodeGraphColor.b, 0.5f));
			Im3d::Vertex(parentPos, Im3d::Color(kNodeGraphColor.r, kNodeGraphColor.g, kNodeGraphColor.b, 0.1f));
		Im3d::End();
	}

	return true;
}

// PUBLIC

SceneEditor::SceneEditor(Scene* _scene_)
	: m_scene(_scene_)
	, m_editNode(0)
	, m_editCamera(0)
	, m_editXForm(0)
	, m_showNodeGraph3d(false)
{
}

void SceneEditor::edit()
{
	ImGui::Begin("Scene Editor", 0,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize
		);

	if (ImGui::TreeNode("Node Counters")) {
		int totalNodes = 0;
		for (int i = 0; i < Node::kTypeCount; ++i) {
			ImGui::Text("%-15s: %d", kNodeTypeStr[i], (int)m_scene->m_nodes[i].size());
			totalNodes = (int)m_scene->m_nodes[i].size();
		}
		ImGui::Spacing();
		ImGui::Text("%Total          : %d", totalNodes);

		ImGui::TreePop();
	}
	ImGui::Spacing();
	ImGui::Checkbox("Show Node Graph", &m_showNodeGraph3d);
	if (m_showNodeGraph3d) {		
		m_scene->traverse(DrawNodeGraphCallback, &m_scene->m_root);
	}

 // NODES
	ImGui::Spacing();
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
			bool destroyNode = false;

			ImGui::SameLine();
			if (ImGui::Button("Destroy")) {
				destroyNode = true;
			 // don't destroy the last root/camera
			 // \todo modal warning when deleting a root or a node with children?
				if (m_editNode->getType() == Node::kTypeRoot || m_editNode->getType() == Node::kTypeCamera) {
					if (m_scene->m_nodes[m_editNode->getType()].size() == 1) {
						APT_LOG_ERR("Error: Can't delete the only %s", kNodeTypeStr[m_editNode->getType()]);
						destroyNode = false;
					}
				}
			}

			ImGui::Separator();
			ImGui::Spacing();
			
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


		 // select parent
		 // \todo check for loops
			ImGui::Spacing();
			ImGui::PushID("SelectParent");
				if (ImGui::Button("Parent ->")) {
					beginSelectNode();
				}
				Node* newParent = selectNode(m_editNode->getParent());
			ImGui::PopID();

			if (newParent != m_editNode->getParent()) {
				m_editNode->setParent(newParent);
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

					if (m_editXForm) {
						if (ImGui::Button("Move Up")) {
							m_editNode->moveXForm(m_editXForm, -1);
						}
						ImGui::SameLine();
						if (ImGui::Button("MoveDown")) {
							m_editNode->moveXForm(m_editXForm, 1);
						}
					}


					/*ImGui::Spacing();
					ImGui::Columns(2);
					ImGui::BeginChild("XForm Stack", ImVec2(0, 128), true);
						for (int i = (int)(m_editNode->m_xforms.size() - 1); i >= 0; --i) {
							XForm* xform = m_editNode->m_xforms[i];
							if (ImGui::Selectable(xform->getName())) {
								newEditXForm = m_editXForm = xform;
							}
						}
					ImGui::EndChild();
					ImGui::NextColumn();
					if (m_editXForm) {
						if (ImGui::Button("Move Up")) {
							m_editNode->moveXForm(m_editXForm, 1);
						}
						if (ImGui::Button("MoveDown")) {
							m_editNode->moveXForm(m_editXForm, -1);
						}
						ImGui::Spacing();
						if (ImGui::Button("Remove")) {
							m_editNode->removeXForm(m_editXForm);
							XForm::Destroy(m_editXForm); // \todo need ref count
						}
					}

					ImGui::Columns(1);*/
				
					if (m_editXForm) {
						ImGui::Spacing();
						ImGui::Spacing();
						m_editXForm->edit();
					}

				}

				if (m_editXForm != newEditXForm) {
					m_editNode->addXForm(newEditXForm);
					m_editXForm = newEditXForm;
				}
				
				ImGui::TreePop();
			}

		 // deferred destroy
			if (destroyNode) {
				if (m_editNode->getType() == Node::kTypeCamera) {
					if (m_editNode->getUserData() == m_editCamera) {
						m_editCamera = 0;
					}
				}
				m_scene->destroyNode(m_editNode);
				newEditNode = 0;
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
		}
	}

 // CAMERAS
	if (ImGui::CollapsingHeader("Cameras")) {

		ImGui::PushID("SelectCamera");
			if (ImGui::Button("Select##Camera")) {
				beginSelectCamera();
			}
			Camera* newEditCamera = selectCamera(m_editCamera);
		ImGui::PopID();

		ImGui::SameLine();
		if (ImGui::Button("Create")) {
			newEditCamera = m_scene->createCamera(Camera());
		}

		if (m_editCamera) {
			bool destroyCamera = false;

			ImGui::SameLine();
			if (ImGui::Button("Destroy")) {
				destroyCamera = true;
				if (m_scene->m_cameras.size() == 1) {
					APT_LOG_ERR("Error: Can't delete the only Camera");
					destroyCamera = false;
				}
			}

			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Set Draw Camera")) {
				m_scene->m_drawCamera = m_editCamera;
			}
			ImGui::SameLine();
			if (ImGui::Button("Set Cull Camera")) {
				m_scene->m_cullCamera = m_editCamera;
			}
			if (ImGui::Button("Set Current Node")) {
			 // deselect any camera nodes
				for (int i = 0; i < m_scene->getNodeCount(Node::kTypeCamera); ++i) {
					Node* node = m_scene->getNode(Node::kTypeCamera, i);
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
			if (destroyCamera) {
				if (m_editNode == m_editCamera->getNode()) {
					m_editNode = 0;
				}
				m_scene->destroyCamera(m_editCamera);
				newEditCamera = 0;
			}
		}

	 // deferred select
		m_editCamera = newEditCamera;

	}
	
 // HIERARCHY
	if (ImGui::CollapsingHeader("Hierarchy")) {
		drawHierarchy(&m_scene->m_root);
	}

	ImGui::End();
}

void SceneEditor::beginSelectNode()
{
	ImGui::OpenPopup("Select Node");
}

Node* SceneEditor::selectNode(Node* _current, Node::Type _type)
{
	Node* ret = _current;
	if (ImGui::BeginPopup("Select Node")) {
		static ImGuiTextFilter filter;
		filter.Draw("Filter##Node");
		int type = _type == Node::kTypeCount ? 0 : _type;
		int typeEnd = APT_MIN((int)_type + 1, (int)Node::kTypeCount);
		for (; type < typeEnd; ++type) {
			for (int node = 0; node < (int)m_scene->m_nodes[type].size(); ++node) {
				String<32> tmp("[%s] %s", kNodeTypeStr[type], m_scene->m_nodes[type][node]->getName());
				if (filter.PassFilter(tmp)) {
					if (ImGui::Selectable(tmp)) {
						ret = m_scene->m_nodes[type][node];
						break;
					}
				}
			}
		}
		ImGui::EndPopup();
	}
	return ret;
}

void SceneEditor::beginSelectCamera()
{
	ImGui::OpenPopup("Select Camera");
}

Camera* SceneEditor::selectCamera(Camera* _current)
{
	Camera* ret = _current;
	if (ImGui::BeginPopup("Select Camera")) {
		static ImGuiTextFilter filter;
		filter.Draw("Filter##Camera");
		for (auto it = m_scene->m_cameras.begin(); it != m_scene->m_cameras.end(); ++it) {
			Camera* cam = *it;
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

void SceneEditor::beginCreateNode()
{
	ImGui::OpenPopup("Create Node");
}
Node* SceneEditor::createNode(Node* _current)
{
	Node* ret = _current;
	if (ImGui::BeginPopup("Create Node")) {
		static const char* kNodeTypeComboStr = "Root\0Camera\0Light\0Object\0";
		static int s_type = Node::kTypeObject;
		ImGui::Combo("Type", &s_type , kNodeTypeComboStr);

		static Node::NameStr s_name;
		ImGui::InputText("Name", s_name, s_name.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue);
		Node::AutoName((Node::Type)s_type, s_name);

		if (ImGui::Button("Create")) {
			ret = m_scene->createNode(0, (Node::Type)s_type);
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

void SceneEditor::beginCreateXForm()
{
	ImGui::OpenPopup("Create XForm");
}

XForm* SceneEditor::createXForm(XForm* _current)
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

void SceneEditor::drawHierarchy(Node* _node, int _depth)
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
				drawHierarchy(_node->getChild(i), _depth + 4);
			}
			ImGui::TreePop();
		}
	}

	ImGui::PopStyleColor();
}

#endif // frm_Scene_ENABLE_EDIT
