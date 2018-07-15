#include <frm/Scene.h>

#include <frm/Camera.h>
#include <frm/Light.h>
#include <frm/Profiler.h>
#include <frm/XForm.h>

#include <apt/log.h>
#include <apt/Json.h>

#include <EASTl/algorithm.h>
#include <EASTL/utility.h> // eastl::swap

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
	for (int i = 0; i < Node::Type_Count; ++i) {
		if (strcmp(kNodeTypeStr[i], _str) == 0) {
			return (Node::Type)i;
		}
	}
	return Node::Type_Count;
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

void Node::moveXForm(const XForm* _xform, int _dir)
{
	for (int i = 0, n = getXFormCount(); i < n; ++i) {
		if (m_xforms[i] == _xform) {
			int j = APT_CLAMP(i + _dir, 0, n - 1);
			eastl::swap(m_xforms[i], m_xforms[j]);
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
	APT_ASSERT(eastl::find(m_children.begin(), m_children.end(), _node) == m_children.end()); // added the same child multiple times?
	m_children.push_back(_node);
	if (_node->m_parent && _node->m_parent != this) {
		_node->m_parent->removeChild(_node);
	}
	_node->m_parent = this;

	if (_node->isStatic()) {
		Update(_node, 0.0f, Node::State_Any);
	}
}

void Node::removeChild(Node* _node)
{
	APT_ASSERT(_node);
	auto it = eastl::find(m_children.begin(), m_children.end(), _node);
	if (it != m_children.end()) {
		(*it)->m_parent = nullptr;
		m_children.erase(it);
	}
}


// PRIVATE

static unsigned s_typeCounters[Node::Type_Count] = {}; // for auto name
void Node::AutoName(Node::Type _type, Node::NameStr& out_)
{
	out_.setf("%s_%03u", kNodeTypeStr[_type], s_typeCounters[_type]);
}

void Node::Update(Node* _node_, float _dt, uint8 _stateMask)
{
	if (!(_node_->m_state & _stateMask)) {
		return;
	}

 // reset world matrix
	_node_->m_worldMatrix = _node_->m_localMatrix;

 // apply xforms
	for (auto& xform : _node_->m_xforms) {
		xform->apply(_dt);
	}

 // move to parent space
	if (_node_->m_parent) {
		_node_->m_worldMatrix = _node_->m_parent->m_worldMatrix * _node_->m_worldMatrix;
	}

 // type-specific update
	switch (_node_->getType()) {
		case Node::Type_Camera: {
			Camera* camera = _node_->getSceneDataCamera();
			APT_ASSERT(camera);
			APT_ASSERT(camera->m_parent == _node_);
			camera->update();
			}
			break;
		default: 
			break;
	};

 // update children
	for (auto& child : _node_->m_children) {
		Update(child, _dt, _stateMask);
	}
}

Node::Node()
	: m_id(kInvalidId)
	, m_type(Type_Count)
	, m_state(0)
	, m_parent(nullptr)
{
}

Node::Node(Type _type, Id _id, uint8 _state, const char* _name)
	: m_id(_id)
	, m_type(_type)
	, m_state(_state)
	, m_userData(0)
	, m_sceneData(0)
	, m_localMatrix(identity)
	, m_parent(nullptr)
{
	APT_ASSERT(_type < Type_Count);
	if (_name) {
		m_name.set(_name);
	} else {
		AutoName(_type, m_name);
		s_typeCounters[_type]++;
	}
}

Node::~Node()
{
 // re-parent children
	for (auto it = m_children.begin(); it != m_children.end(); ++it) {
		Node* n = *it;
		n->m_parent = nullptr; // prevent m_parent->addChild calling removeChild on this (invalidates it)
		if (m_parent) {
			m_parent->addChild(n);
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
	eastl::swap(m_xforms[_i], m_xforms[j]);
	return j;
}


/*******************************************************************************

                                   Scene

*******************************************************************************/
Scene* Scene::s_currentScene;

void frm::swap(Scene& _a, Scene& _b)
{
	eastl::swap(_a.m_nextNodeId, _b.m_nextNodeId);
	eastl::swap(_a.m_root,       _b.m_root);
	eastl::swap(_a.m_nodes,      _b.m_nodes);
	apt::swap(_a.m_nodePool,   _b.m_nodePool);
	eastl::swap(_a.m_drawCamera, _b.m_drawCamera);
	eastl::swap(_a.m_cullCamera, _b.m_cullCamera);
	eastl::swap(_a.m_cameras,    _b.m_cameras);
	apt::swap(_a.m_cameraPool, _b.m_cameraPool);
	eastl::swap(_a.m_lights,    _b.m_lights);
	apt::swap(_a.m_lightPool, _b.m_lightPool);
}


// PUBLIC

bool Scene::Load(const char* _path, Scene& scene_)
{
	APT_LOG("Loading scene from '%s'", _path);
	Json json;
	if (!Json::Read(json, _path)) {
		return false;
	}
	SerializerJson serializer(json, SerializerJson::Mode_Read);
	Scene newScene;
	
	if (!Serialize(serializer, newScene)) {
		return false;
	}
	swap(newScene, scene_);
	return true;
}

bool Scene::Save(const char* _path, Scene& _scene)
{
	APT_LOG("Saving scene to '%s'", _path);
	Json json;
	SerializerJson serializer(json, SerializerJson::Mode_Write);
	if (!Serialize(serializer, _scene)) {
		return false;
	}
	return Json::Write(json, _path);
}

Scene::Scene()
	: m_nextNodeId(0)
	, m_nodePool(128)
	, m_cameraPool(8)
	, m_lightPool(16)
	, m_drawCamera(nullptr)
	, m_cullCamera(nullptr)
#ifdef frm_Scene_ENABLE_EDIT
	, m_showNodeGraph3d(false)
	, m_editNode(nullptr)
	, m_storedNode(nullptr)
	, m_editXForm(nullptr)
	, m_editCamera(nullptr)
	, m_storedCullCamera(nullptr)
	, m_storedDrawCamera(nullptr)
	, m_editLight(nullptr)
#endif
{
	m_root = m_nodePool.alloc(Node(Node::Type_Root, m_nextNodeId++, Node::State_Any, "ROOT"));
	m_root->setSceneDataScene(this);
	m_nodes[Node::Type_Root].push_back(m_root);
}

Scene::~Scene()
{
	while (!m_lights.empty()) {
		m_lightPool.free(m_lights.back());
		m_lights.pop_back();
	}
	while (!m_cameras.empty()) {
		m_cameraPool.free(m_cameras.back());
		m_cameras.pop_back();
	}
	for (int i = 0; i < Node::Type_Count; ++i) {
		while (!m_nodes[i].empty()) {
			m_nodePool.free(m_nodes[i].back());
			m_nodes[i].pop_back();
		}
	}
}

void Scene::update(float _dt, uint8 _stateMask)
{
	PROFILER_MARKER_CPU("#Scene::update");
	
	Node::Update(m_root, _dt, _stateMask);
}

bool Scene::traverse(Node* _root_, uint8 _stateMask, OnVisit* _callback)
{
	PROFILER_MARKER_CPU("#Scene::traverse");

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
	PROFILER_MARKER_CPU("#Scene::createNode");

	Node* ret = m_nodePool.alloc(Node(_type, m_nextNodeId++, Node::State_Active));
	if (_type == Node::Type_Camera || _type == Node::Type_Root) {
		ret->setDynamic(true);
	}
	_parent = _parent ? _parent : m_root;
	_parent->addChild(ret);
	m_nodes[_type].push_back(ret);
	return ret;
}

void Scene::destroyNode(Node*& _node_)
{
	PROFILER_MARKER_CPU("#Scene::destroyNode");

	APT_ASSERT(_node_ != m_root); // can't destroy the root
	
	Node::Type type = _node_->getType();
	switch (type) {
		case Node::Type_Camera:
			if (_node_->m_sceneData) {
				Camera* camera = _node_->getSceneDataCamera();
				auto it = eastl::find(m_cameras.begin(), m_cameras.end(), camera);
				if (it != m_cameras.end()) {
					APT_ASSERT(camera->m_parent == _node_); // _node_ points to camera, but camera doesn't point to _node_
					m_cameras.erase(it);
				}
				m_cameraPool.free(camera);
			}
			break;
		case Node::Type_Light:
			if (_node_->m_sceneData) {
				Light* light = _node_->getSceneDataLight();
				auto it = eastl::find(m_lights.begin(), m_lights.end(), light);
				if (it != m_lights.end()) {
					APT_ASSERT(light->m_parent == _node_); // _node_ points to light, but light doesn't point to _node_
					m_lights.erase(it);
				}
				m_lightPool.free(light);
			}
			break;
		default:
			break;
	};

	auto it = eastl::find(m_nodes[type].begin(), m_nodes[type].end(), _node_);
	if (it != m_nodes[type].end()) {
		m_nodes[type].erase(it);
		m_nodePool.free(_node_);
		_node_ = nullptr;
	}
}

Node* Scene::findNode(Node::Id _id, Node::Type _typeHint)
{
	PROFILER_MARKER_CPU("#Scene::findNode");

	Node* ret = nullptr;
	if (_typeHint != Node::Type_Count) {
		for (auto it = m_nodes[_typeHint].begin(); it != m_nodes[_typeHint].end(); ++it) {
			if ((*it)->getId() == _id) {
				ret = *it;
				break;
			}
		}
	}
	for (int i = 0; ret == nullptr && i < Node::Type_Count; ++i) {
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

Node* Scene::findNode(const char* _name, Node::Type _typeHint)
{
	PROFILER_MARKER_CPU("#Scene::findNode");

	Node* ret = nullptr;
	if (_typeHint != Node::Type_Count) {
		for (auto it = m_nodes[_typeHint].begin(); it != m_nodes[_typeHint].end(); ++it) {
			if (strcmp((*it)->getName(), _name) == 0) {
				ret = *it;
				break;
			}
		}
	}
	for (int i = 0; ret == nullptr && i < Node::Type_Count; ++i) {
		if (i == _typeHint) {
			continue;
		}
		for (auto it = m_nodes[i].begin(); it != m_nodes[i].end(); ++it) {
			if (strcmp((*it)->getName(), _name) == 0) {
				ret = *it;
				break;
			}
		}
	}
	return ret;
}

Camera* Scene::createCamera(const Camera& _copyFrom, Node* _parent_)
{
	PROFILER_MARKER_CPU("#Scene::createCamera");

	Camera* ret = m_cameraPool.alloc(_copyFrom);
	Node* node = createNode(Node::Type_Camera, _parent_);
	node->setSceneDataCamera(ret);
	ret->m_parent = node;

	m_cameras.push_back(ret);
	ret->updateGpuBuffer();
	if (!m_drawCamera) {
		m_drawCamera = ret;
		m_cullCamera = ret;
	}
	return ret;
}

void Scene::destroyCamera(Camera*& _camera_)
{
	PROFILER_MARKER_CPU("#Scene::destroyCamera");

	Node* node = _camera_->m_parent;
	APT_ASSERT(node);
	destroyNode(node); // implicitly destroys camera
	if (m_editCamera == _camera_) {
		m_editCamera = nullptr;
	}
	if (m_drawCamera == _camera_) {
		m_drawCamera = nullptr;
	}
	if (m_cullCamera == _camera_) {
		m_cullCamera = nullptr;
	}	
	_camera_ = nullptr;
}

Light* Scene::createLight(Node* _parent_)
{
	PROFILER_MARKER_CPU("#Scene::createLight");

	Light* ret = m_lightPool.alloc();
	Node* node = createNode(Node::Type_Light, _parent_);
	node->setSceneDataLight(ret);
	ret->m_parent = node;
	m_lights.push_back(ret);
	return ret;
}

void Scene::destroyLight(Light*& _light_)
{
	PROFILER_MARKER_CPU("#Scene::destroyLight");

	Node* node = _light_->m_parent;
	APT_ASSERT(node);
	destroyNode(node); // implicitly destroys light
	if (m_editLight == _light_) {
		m_editLight = nullptr;
	}
	m_editLight = nullptr;
}

bool frm::Serialize(Serializer& _serializer_, Scene& _scene_)
{
	bool ret = true;

	ret &= Serialize(_serializer_, _scene_, *_scene_.m_root);
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		#ifdef frm_Scene_ENABLE_EDIT
			_scene_.m_editNode   = nullptr;
			_scene_.m_editXForm  = nullptr;
			_scene_.m_editCamera = nullptr;
		#endif
	}

	Node::Id drawCameraId = Node::kInvalidId;
	Node::Id cullCameraId = Node::kInvalidId;
	if (_serializer_.getMode() == Serializer::Mode_Write) {
		if (_scene_.m_drawCamera && _scene_.m_drawCamera->m_parent) {
			drawCameraId = _scene_.m_drawCamera->m_parent->getId();	
		}
		if (_scene_.m_cullCamera && _scene_.m_cullCamera->m_parent) {
			cullCameraId = _scene_.m_cullCamera->m_parent->getId();
		}
	}
	ret &= Serialize(_serializer_, drawCameraId, "DrawCameraId");
	ret &= Serialize(_serializer_, cullCameraId, "CullCameraId");
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		if (drawCameraId != Node::kInvalidId) {
			Node* n = _scene_.findNode(drawCameraId, Node::Type_Camera);
			if (n != nullptr) {
				_scene_.m_drawCamera = n->getSceneDataCamera();
			}
		}
		if (cullCameraId != Node::kInvalidId) {
			Node* n = _scene_.findNode(cullCameraId, Node::Type_Camera);
			if (n != nullptr) {
				_scene_.m_cullCamera = n->getSceneDataCamera();
			}
		}

		for (int i = 0; i < Node::Type_Count; ++i) {
			s_typeCounters[i] = APT_MAX((unsigned int)_scene_.m_nodes[i].size(), s_typeCounters[i]);
		}
	}

	APT_ASSERT(_scene_.m_drawCamera != nullptr);
	if (_scene_.m_cullCamera == nullptr) {
		_scene_.m_cullCamera = _scene_.m_drawCamera;
	}

	return ret;
}

bool frm::Serialize(Serializer& _serializer_, Scene& _scene_, Node& _node_)
{
	bool ret = true;

	ret &= Serialize(_serializer_, _node_.m_id,   "Id");
	ret &= Serialize(_serializer_, _node_.m_name, "Name");
	
	bool active   = _node_.isActive();
	bool dynamic  = _node_.isDynamic();
	bool selected = _node_.isSelected();
	ret &= Serialize(_serializer_, active,   "Active");
	ret &= Serialize(_serializer_, dynamic,  "Dynamic");
	ret &= Serialize(_serializer_, selected, "Selected");
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		_node_.setActive(active);
		_node_.setDynamic(dynamic);
		_node_.setSelected(selected);
	}

	ret &= Serialize(_serializer_, _node_.m_userData,    "UserData");
	ret &= Serialize(_serializer_, _node_.m_localMatrix, "LocalMatrix");

	String<64> typeStr = kNodeTypeStr[_node_.m_type];
	ret &= Serialize(_serializer_, typeStr, "Type");
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		_node_.m_type = NodeTypeFromStr((const char*)typeStr);
		if (_node_.m_type == Node::Type_Count) {
			APT_LOG_ERR("Scene: Invalid node type '%s'", (const char*)typeStr);
			return false;
		}

		switch (_node_.m_type) {
			case Node::Type_Root: {
				_node_.setSceneDataScene(&_scene_);
				break;
			}
			case Node::Type_Camera: {
				Camera* cam = _scene_.m_cameraPool.alloc();
				cam->m_parent = &_node_;
				if (!Serialize(_serializer_, *cam)) {
					_scene_.m_cameraPool.free(cam);
					return false;
				}
				_scene_.m_cameras.push_back(cam);
				_node_.setSceneDataCamera(cam);
				break;
			}
			case Node::Type_Light: {
				Light* light = _scene_.m_lightPool.alloc();
				light->m_parent = &_node_;
				if (!Serialize(_serializer_, *light)) {
					_scene_.m_lightPool.free(light);
					return false;
				}
				_scene_.m_lights.push_back(light);
				_node_.setSceneDataLight(light);
				break;
			}
			default:
				break;
		};
		_scene_.m_nextNodeId = APT_MAX(_scene_.m_nextNodeId, _node_.m_id + 1);

		uint childCount = (uint)_node_.getChildCount();
		if (_serializer_.beginArray(childCount, "Children")) {
			while (_serializer_.beginObject()) {
				Node* child = _scene_.m_nodePool.alloc(Node());
				if (!Serialize(_serializer_, _scene_, *child)) {
					_scene_.m_nodePool.free(child);
					return false;
				}
				child->m_parent = &_node_;
				_node_.m_children.push_back(child);
				_scene_.m_nodes[child->m_type].push_back(child);
				_serializer_.endObject();
			}
			_serializer_.endArray();
		}

		uint xformCount = (uint)_node_.getXFormCount();
		if (_serializer_.beginArray(xformCount, "XForms")) {
			while (_serializer_.beginObject()) {
				String<64> className;
				if (!Serialize(_serializer_, className, "Class")) {
					return false;
				}
				XForm* xform = XForm::Create(StringHash((const char*)className));
				if (xform) {
					xform->serialize(_serializer_);
					xform->setNode(&_node_);
					_node_.m_xforms.push_back(xform);
				} else {
					APT_LOG_ERR("Scene: Invalid xform '%s'", (const char*)className);
				}
				_serializer_.endObject();
			}
			_serializer_.endArray();
		}
		
	} else { // if writing
		switch (_node_.m_type) {
			case Node::Type_Camera: {
				Camera* cam = _node_.getSceneDataCamera();
				if (!Serialize(_serializer_, *cam)) {
					return false;
				}
				break;
			}
			case Node::Type_Light: {
				Light* light = _node_.getSceneDataLight();
				if (!Serialize(_serializer_, *light)) {
					return false;
				}
				break;
			}
			default:
				break;
		};

	 // \todo childCount is incorrect as '#' nodes aren't serialized
		uint childCount = (uint)_node_.getChildCount();
		if (!_node_.m_children.empty()) {
			_serializer_.beginArray(childCount, "Children");
				for (auto& child : _node_.m_children) {
					if (child->getName()[0] == '#') {
						continue;
					}
					_serializer_.beginObject();
						Serialize(_serializer_, _scene_, *child);
					_serializer_.endObject();
				}
			_serializer_.endArray();
		}

		uint xformCount = (uint)_node_.getXFormCount();
		if (!_node_.m_xforms.empty()) {
			_serializer_.beginArray(xformCount, "XForms");
				for (auto& xform : _node_.m_xforms) {
					_serializer_.beginObject();
						String<64> className = xform->getClassRef()->getName();
						Serialize(_serializer_, className, "Class");
						xform->serialize(_serializer_);
					_serializer_.endObject();
				}
			_serializer_.endArray();
		}
	}

	return true;
}

#ifdef frm_Scene_ENABLE_EDIT

#include <im3d/Im3d.h>
#include <imgui/imgui.h>

static const char* kNodeTypeIconStr[] =
{
	ICON_FA_COG,          // root
	ICON_FA_VIDEO_CAMERA, // camera
	ICON_FA_CUBE,         // object
	ICON_FA_LIGHTBULB_O   // light
};

static const Im3d::Color kNodeTypeCol[] = 
{
	Im3d::Color(0.5f, 0.5f, 0.5f, 0.5f), // Type_Root,
	Im3d::Color(0.5f, 0.5f, 1.0f, 0.5f), // Type_Camera,
	Im3d::Color(0.5f, 1.0f, 0.5f, 1.0f), // kTypeObject,
	Im3d::Color(1.0f, 0.5f, 0.0f, 1.0f)  // kTypeLight,
};

void Scene::edit()
{
	ImGui::Begin("Scene", 0, 
		ImGuiWindowFlags_NoResize | 
		ImGuiWindowFlags_AlwaysAutoResize
		);

	if (ImGui::TreeNode("Scene Info")) {
		int totalNodes = 0;
		for (int i = 0; i < Node::Type_Count; ++i) {
			ImGui::Text("%d %s ", (int)m_nodes[i].size(), kNodeTypeIconStr[i]);
			ImGui::SameLine();
			totalNodes += (int)m_nodes[i].size();
		}
		ImGui::Spacing();

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
			m_root, Node::State_Any, 
			[](Node* _node_)->bool {
				Im3d::SetMatrix(_node_->getWorldMatrix());
				Im3d::DrawXyzAxes();
				//Im3d::BeginPoints();
				//	Im3d::Vertex(Im3d::Vec3(0.0f), 4.0f, kNodeTypeCol[_node_->getType()]);
				//Im3d::End();
				Im3d::SetIdentity();
				if (_node_->getParent() && _node_->getParent() != Scene::GetCurrent()->getRoot()) {
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

	ImGui::Spacing();
	editLights();

	ImGui::End(); // Scene
}

void Scene::editNodes()
{
	if (ImGui::CollapsingHeader("Nodes")) {
		ImGui::PushID("SelectNode");
			if (ImGui::Button(ICON_FA_LIST_UL " Select")) {
				beginSelectNode();
			}
			Node* newEditNode = selectNode(m_editNode);
		ImGui::PopID();

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_FILE_O " Create")) {
			beginCreateNode();
		}
		newEditNode = createNode(newEditNode);

		if (m_editNode) {
			bool destroyNode = false;

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TIMES " Destroy")) {
				destroyNode = true;
			 // don't destroy the last root/camera
			 // \todo modal warning when deleting a root or a node with children?
				if (m_editNode->getType() == Node::Type_Root || m_editNode->getType() == Node::Type_Camera) {
					if (m_nodes[m_editNode->getType()].size() == 1) {
						APT_LOG_ERR("Error: Can't delete the only %s", kNodeTypeStr[m_editNode->getType()]);
						destroyNode = false;
					}
				}
			}

			ImGui::Separator();
			ImGui::Spacing();
			static Node::NameStr s_nameBuf;
			s_nameBuf.set((const char*)m_editNode->m_name);
			if (ImGui::InputText("Name", (char*)s_nameBuf, s_nameBuf.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)) {
				m_editNode->m_name.set((const char*)s_nameBuf);
			}

			bool active = m_editNode->isActive();
			bool dynamic = m_editNode->isDynamic();
			if (ImGui::Checkbox("Active", &active)) {
				m_editNode->setActive(active);
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Dynamic", &dynamic)) {
				m_editNode->setDynamic(dynamic);
			}

		 // \todo check for loops?
			ImGui::Spacing();
			ImGui::PushID("SelectParent");
				if (ImGui::Button(ICON_FA_LINK " Parent")) {
					beginSelectNode();
				}
				Node* newParent = selectNode(m_editNode->getParent());
				if (newParent == m_editNode) {
					APT_LOG_ERR("Error: Can't parent a node to itself");
					newParent = m_editNode->getParent();
				}
			ImGui::PopID();

			if (newParent != m_editNode->getParent()) {
			 // maintain child world space position when changing parent
				mat4 parentWorld = m_editNode->m_parent ? m_editNode->m_parent->m_worldMatrix : identity;
				mat4 childWorld = parentWorld * m_editNode->m_localMatrix;
				m_editNode->setParent(newParent);
				parentWorld = m_editNode->m_parent ? m_editNode->m_parent->m_worldMatrix : identity;
				m_editNode->m_localMatrix = inverse(parentWorld) * childWorld;
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

			if (!m_editNode->m_children.empty()) {
				ImGui::Spacing();
				if (ImGui::TreeNode("Children")) {
					for (auto it = m_editNode->m_children.begin(); it != m_editNode->m_children.end(); ++it) {
						Node* child = *it;
						ImGui::Text("%s %s", kNodeTypeIconStr[child->getType()], child->getName());
						if (ImGui::IsItemClicked()) {
							newEditNode = child;
							break;
						}
					}

					ImGui::TreePop();
				}
			}

			if (ImGui::TreeNode("Local Matrix")) {
			 // hierarchical update - modify the world space node and transform back into parent space
				mat4 parentWorld = m_editNode->m_parent ? m_editNode->m_parent->m_worldMatrix : identity;
				mat4 childWorld = parentWorld * m_editNode->m_localMatrix;
				if (Im3d::Gizmo("GizmoNodeLocal", (float*)&childWorld)) {
					m_editNode->m_localMatrix = inverse(parentWorld) * childWorld;
					Node::Update(m_editNode, 0.0f, Node::State_Any); // force node update
				}

				vec3 position = GetTranslation(m_editNode->m_localMatrix);
				vec3 rotation = ToEulerXYZ(GetRotation(m_editNode->m_localMatrix));
				vec3 scale    = GetScale(m_editNode->m_localMatrix);
				ImGui::Text("Position: %.3f, %.3f, %.3f", position.x, position.y, position.z);
				ImGui::Text("Rotation: %.3f, %.3f, %.3f", Degrees(rotation.x), Degrees(position.y), Degrees(position.z));
				ImGui::Text("Scale:    %.3f, %.3f, %.3f", scale.x, scale.y, scale.z);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("XForms")) {
				bool destroyXForm = false;

				if (ImGui::Button(ICON_FA_FILE_O " Create")) {
					beginCreateXForm();
				}
				XForm* newEditXForm = createXForm(m_editXForm);
				if (newEditXForm != m_editXForm) {
					m_editNode->addXForm(newEditXForm);
				}
				if (m_editXForm != nullptr) {
					ImGui::SameLine();
					if (ImGui::Button(ICON_FA_TIMES " Destroy")) {
						destroyXForm = true;
					}
					ImGui::SameLine();
					if (ImGui::Button(ICON_FA_ARROW_UP)) {
						m_editNode->moveXForm(m_editXForm, -1);
					}
					ImGui::SameLine();
					if (ImGui::Button(ICON_FA_ARROW_DOWN)) {
						m_editNode->moveXForm(m_editXForm, 1);
					}
				}

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
						newEditXForm = m_editNode->m_xforms[selectedXForm];
					}

					if (m_editXForm) {
						ImGui::Separator();
						ImGui::Spacing();
						ImGui::PushID(m_editXForm);
							m_editXForm->edit();
						ImGui::PopID();
					}

				}

				if (destroyXForm) {
					m_editNode->removeXForm(m_editXForm);
					XForm::Destroy(m_editXForm);
					newEditXForm = nullptr;
				}

				if (m_editXForm != newEditXForm) {
					m_editXForm = newEditXForm;
				}

				ImGui::TreePop();
			}

		 // deferred destroy
			if (destroyNode) {
				if (m_editNode->getType() == Node::Type_Camera) {
				 // destroyNode will implicitly destroy camera, so deselect camera if selected
					if (m_editNode->getSceneDataCamera() == m_editCamera) {
						m_editCamera = nullptr;
					}
				}
				Scene::destroyNode(m_editNode);
				newEditNode = nullptr;
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
			switch (newEditNode->m_type) {
				case Node::Type_Camera:
					m_editCamera = newEditNode->getSceneDataCamera();
					break;
				case Node::Type_Light:
					m_editLight = newEditNode->getSceneDataLight();
					break;
			};
			m_editNode = newEditNode;
			m_editXForm = nullptr;
		}
	}
}

void Scene::editCameras()
{
	if (ImGui::CollapsingHeader("Cameras")) {
		ImGui::PushID("SelectCamera");
			if (ImGui::Button(ICON_FA_LIST_UL " Select##Camera")) {
				beginSelectCamera();
			}
			Camera* newEditCamera = selectCamera(m_editCamera);
		ImGui::PopID();

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_FILE_O " Create")) {
			newEditCamera = createCamera(Camera());
		}

		if (m_editCamera) {
			bool destroy = false;

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TIMES " Destroy")) {
				destroy = true;
				if (m_cameras.size() == 1) {
					APT_LOG_ERR("Error: Can't delete the only Camera");
					destroy = false;
				}
			}

			ImGui::Separator();

			ImGui::PushStyleColor(ImGuiCol_Text, m_drawCamera == m_editCamera ? (ImVec4)ImColor(0xff3380ff) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
			if (ImGui::Button(ICON_FA_VIDEO_CAMERA " Set Draw Camera")) {
				if (m_drawCamera == m_editCamera && m_storedDrawCamera != nullptr) {
					m_drawCamera = m_storedDrawCamera;
				} else {
					m_storedDrawCamera = m_drawCamera;
					m_drawCamera = m_editCamera;
				}
			}
			ImGui::PopStyleColor();
			
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, m_cullCamera == m_editCamera ? (ImVec4)ImColor(0xff3380ff) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
			if (ImGui::Button(ICON_FA_CUBES " Set Cull Camera")) {
				if (m_cullCamera == m_editCamera && m_storedCullCamera != nullptr) {
					m_cullCamera = m_storedCullCamera;
				} else {
					m_storedCullCamera = m_cullCamera;
					m_cullCamera = m_editCamera;
				}
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, m_editCamera->m_parent->isSelected() ? (ImVec4)ImColor(0xff3380ff) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
			if (ImGui::Button(ICON_FA_GAMEPAD " Set Current Node")) {
			 	Node* editCameraNode = m_editCamera->m_parent;
				if (editCameraNode->isSelected() && m_storedNode != nullptr) {
					editCameraNode->setSelected(false);
					m_storedNode->setSelected(true);
				} else {
				 // deselect any camera nodes \todo potentially buggy
					for (int i = 0; i < getNodeCount(Node::Type_Camera); ++i) {
						Node* node = getNode(Node::Type_Camera, i);
						if (node->isSelected()) {
							m_storedNode = node;
							node->setSelected(false);
							break;
						}
					}
					editCameraNode->setSelected(true);
				}
			}
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::Spacing();
			
			static Node::NameStr s_nameBuf;
			s_nameBuf.set((const char*)m_editCamera->m_parent->m_name);
			if (ImGui::InputText("Name", (char*)s_nameBuf, s_nameBuf.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)) {
				m_editCamera->m_parent->m_name.set((const char*)s_nameBuf);
			}

			m_editCamera->edit();

		 // deferred destroy
			if (destroy) {
				if (m_editNode == m_editCamera->m_parent) {
					m_editNode = nullptr;
				}
				destroyCamera(m_editCamera);
				newEditCamera = m_cameras[0];

			 // reset stored cameras
				if (m_storedDrawCamera == m_editCamera) {
					m_storedDrawCamera = nullptr;
				}
				if (m_storedCullCamera == m_editCamera) {
					m_storedCullCamera = nullptr;
				}

			 // reset draw/cull cameras
				if (m_drawCamera == m_editCamera) {
					m_drawCamera = m_storedDrawCamera ? m_storedDrawCamera : m_cameras[0];
				}
				if (m_cullCamera == m_editCamera) {
					m_cullCamera = m_storedCullCamera ? m_storedCullCamera : m_cameras[0];
				}
			}
		}
	 // deferred select
		if (m_editCamera != newEditCamera) {
			if (newEditCamera->m_parent) {
				m_editNode = newEditCamera->m_parent;
			}
			m_editCamera = newEditCamera;
		}
		
	}
}

void Scene::editLights()
{
	if (ImGui::CollapsingHeader("Lights")) {
		ImGui::PushID("SelectLight");
			if (ImGui::Button(ICON_FA_LIST_UL " Select##Light")) {
				beginSelectLight();
			}
			Light* newEditLight = selectLight(m_editLight);
		ImGui::PopID();

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_FILE_O " Create")) {
			newEditLight = createLight();
		}

		if (m_editLight) {
			bool destroy = false;

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TIMES " Destroy")) {
				destroy = true;
			}

			ImGui::Separator();			
			ImGui::Spacing();
			
			static Node::NameStr s_nameBuf;
			s_nameBuf.set((const char*)m_editLight->m_parent->m_name);
			if (ImGui::InputText("Name", (char*)s_nameBuf, s_nameBuf.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)) {
				m_editLight->m_parent->m_name.set((const char*)s_nameBuf);
			}

			m_editLight->edit();

		 // deferred destroy
			if (destroy) {
				if (m_editNode == m_editLight->m_parent) {
					m_editNode = nullptr;
				}
				destroyLight(m_editLight);
				newEditLight = nullptr;
			}
		}
	 // deferred select
		if (m_editLight != newEditLight) {
			if (newEditLight->m_parent) {
				m_editNode = newEditLight->m_parent;
			}
			m_editLight = newEditLight;
		}
		
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
		int type = _type == Node::Type_Count ? 0 : _type;
		int typeEnd = APT_MIN((int)_type + 1, (int)Node::Type_Count);
		for (; type < typeEnd; ++type) {
			for (int node = 0; node < (int)m_nodes[type].size(); ++node) {
				if (m_nodes[type][node] == _current) {
					continue;
				}
				String<32> tmp("%s %s", kNodeTypeIconStr[type], m_nodes[type][node]->getName());
				if (filter.PassFilter((const char*)tmp)) {
					if (ImGui::Selectable((const char*)tmp)) {
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
			APT_ASSERT(cam->m_parent);
			if (filter.PassFilter(cam->m_parent->getName())) {
				if (ImGui::Selectable(cam->m_parent->getName())) {
					ret = cam;
					break;
				}
			}
		}
		
		ImGui::EndPopup();
	}
	return ret;
}

void Scene::beginSelectLight()
{
	ImGui::OpenPopup("Select Light");
}
Light* Scene::selectLight(Light* _current)
{
	Light* ret = _current;
	if (ImGui::BeginPopup("Select Light")) {
		static ImGuiTextFilter filter;
		filter.Draw("Filter##Light");
		for (auto it = m_lights.begin(); it != m_lights.end(); ++it) {
			Light* light = *it;
			if (light == _current) {
				continue;
			}
			APT_ASSERT(light->m_parent);
			if (filter.PassFilter(light->m_parent->getName())) {
				if (ImGui::Selectable(light->m_parent->getName())) {
					ret = light;
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
		static const char* kNodeTypeComboStr = ICON_FA_COG " Root\0" ICON_FA_VIDEO_CAMERA " Camera\0" ICON_FA_CUBE " Object\0" ICON_FA_LIGHTBULB_O " Light\0";
		static int s_type = Node::Type_Object;
		ImGui::Combo("Type", &s_type , kNodeTypeComboStr);

		static Node::NameStr s_name;
		ImGui::InputText("Name", (char*)s_name, s_name.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue);
		Node::AutoName((Node::Type)s_type, s_name);

		if (ImGui::Button("Create")) {
			ret = createNode((Node::Type)s_type);
			ret->setName((const char*)s_name);
			ret->setStateMask(Node::State_Active | Node::State_Dynamic | Node::State_Selected);
			switch (ret->getType()) {
				case Node::Type_Root:   ret->setSceneDataScene(this); break;
				case Node::Type_Camera: APT_ASSERT(false); break; // \todo creata a camera in this case?
				case Node::Type_Light:  APT_ASSERT(false); break; // \todo creata a light in this case?
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
	String<32> tmp("%s %s", kNodeTypeIconStr[_node->getType()], _node->getName());
	if (m_editNode == _node) {
		tmp.append(" " ICON_FA_CARET_LEFT);
	}
	if (_node->getType() == Node::Type_Camera && _node->isSelected()) {
		tmp.append(" " ICON_FA_GAMEPAD);
	}
	if (_node->getType() == Node::Type_Camera && m_drawCamera == _node->getSceneDataCamera()) {
		tmp.append(" " ICON_FA_VIDEO_CAMERA);
	}
	if (_node->getType() == Node::Type_Camera && m_cullCamera == _node->getSceneDataCamera()) {
		tmp.append(" " ICON_FA_CUBES);
	}
	ImVec4 col = ImColor(0.1f, 0.1f, 0.1f, 1.0f); // = inactive
	if (_node->isActive()) {
		if (_node->isDynamic()) {
			col = ImColor(0.0f, 1.0f, 0.0f); // = active, dynamic
		} else {
			col = ImColor(1.0f, 1.0f, 0.0f); // = active, static
		}
	}
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	if (_node->getChildCount() == 0) {
		ImGui::Text((const char*)tmp);
	} else {
		if (ImGui::TreeNode((const char*)tmp)) {
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
