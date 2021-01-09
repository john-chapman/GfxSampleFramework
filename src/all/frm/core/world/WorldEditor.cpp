#include "WorldEditor.h"

#include <frm/core/Log.h>
#include <frm/core/AppSample.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Input.h>
#include <frm/core/Json.h>
#include <frm/core/String.h>
#include <frm/core/StringHash.h>
#include <frm/core/Window.h>
#include <frm/core/world/World.h>
#include <frm/core/world/components/Component.h>

#include <imgui/imgui.h>
#include <imgui/imgui_ext.h>
#include <im3d/im3d.h>

static inline frm::String<48> GetNodeLabel(const frm::SceneNode* _node, bool _showUID)
{
	if (_showUID)
	{
		if (_node->getFlag(frm::SceneNode::Flag::Transient))
		{
			return frm::String<48>("%-24s [~]", _node->getName(), _node->getID());
		}
		else
		{
			return frm::String<48>("%-24s [%04X]", _node->getName(), _node->getID());
		}
	}
	else
	{
		return frm::String<48>("%-24s", _node->getName());
	}
}

static inline bool SelectRelativePath(frm::PathStr& _path_, const char* _extension)
{
	frm::PathStr newPath = _path_;
	frm::String<32> filter("*.%s", _extension);
	if (frm::FileSystem::PlatformSelect(newPath, { filter.c_str() }))
	{
		_path_ = frm::FileSystem::MakeRelative(newPath.c_str());
		frm::FileSystem::SetExtension(_path_, _extension);
		return true;
	}

	return false;
}

static const frm::vec3 kDisabledButtonColor        = frm::vec3(0.5f);
static const frm::vec3 kCreateButtonColor          = frm::vec3(0.231f, 0.568f, 0.188f);
static const frm::vec3 kDuplicateButtonColor       = frm::vec3(0.188f, 0.568f, 0.427f);
static const frm::vec3 kDestroyButtonColor         = frm::vec3(0.792f, 0.184f, 0.184f);
static const frm::vec3 kCreateComponentButtonColor = frm::vec3(0.701f, 0.419f, 0.058f);
static const frm::vec3 kNodeSelectButtonColor      = frm::vec3(0.000f, 0.341f, 0.800f);
static const frm::vec3 kTextLinkColor              = frm::vec3(0.5f,   0.7f,   1.0f);

static inline bool TextLink(const char* _text)
{
	//ImGui::TextColored(kTextLinkColor, "%s %s", _text, ICON_FA_EXTERNAL_LINK);
	//return ImGui::IsItemClicked();

	return ImGui::Selectable(_text);
}

static inline bool PrettyButton(const char* _text, const frm::vec3& _color, bool _enabled = true, const frm::vec2& _size = frm::vec2(0.0f))
{
	const frm::vec3 buttonColor  = _enabled ? _color                       : kDisabledButtonColor;
	const frm::vec3 hoveredColor = _enabled ? frm::Saturate(_color * 1.2f) : kDisabledButtonColor;
	const frm::vec3 activeColor  = _enabled ? frm::Saturate(_color * 0.8f) : kDisabledButtonColor;
	const float alpha = _enabled ? 1.0f : 0.5f;

	ImGui::PushStyleColor(ImGuiCol_Button,        frm::vec4(buttonColor,  alpha));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, frm::vec4(hoveredColor, alpha));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  frm::vec4(activeColor,  alpha));
	ImGui::PushStyleColor(ImGuiCol_Text, _enabled ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(0,0,0,1));

	const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x * 2.0f, framePadding.y * 2.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f); 

	bool ret = ImGui::Button(_text, _size);
	
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(4);

	return ret && _enabled;
}

namespace frm {

// PUBLIC

WorldEditor::WorldEditor()
{
	pushAction(ActionType::Edit);
	SetCurrent(this);
}

WorldEditor::~WorldEditor()
{
}

bool WorldEditor::edit()
{
	bool ret = false;
	m_hoveredNode = nullptr;

	m_flash = Max(m_flash - (float)AppSample::GetCurrent()->getDeltaTime() * 2.0f, 0.0f);

	String<32> windowTitle = "World Editor";
	if (m_currentWorld && !m_currentWorld->m_path.isEmpty())
	{
		windowTitle.appendf(" -- '%s'", m_currentWorld->m_path.c_str());
	}
	windowTitle.append("###WorldEditor");

	vec4 windowTitleColor = (vec4)ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive) * (1.0f - m_flash) + vec4(1.0f, 0.0f, 1.0f, 1.0f) * m_flash;
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, windowTitleColor);
	ImGui::Begin(windowTitle.c_str(), nullptr, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar())
	{
		ret |= worldMenu();
		ret |= sceneMenu();
		ret |= viewMenu();		

		ImGui::EndMenuBar();
	}

	if (ImGui::BeginChild("HierarchyView", ImVec2(0.0f, m_hierarchyViewHeight), true))
	{
		ret |= hierarchyView(m_currentWorld->m_rootScene->m_root.referent);
	}
	ImGui::EndChild();

	// \todo generalize splitter behavior
	ImGui::InvisibleButton("HierarchyViewSplitter", ImVec2(-1.0f, 12.0f));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}
	if (ImGui::IsItemActive())
	{
		m_hierarchyViewHeight = Max(64.0f, m_hierarchyViewHeight + ImGui::GetIO().MouseDelta.y);
	}
	
	if (ImGui::BeginChild("EditorView", ImVec2(-1.0f, -1.0f), true))
	{
		ret |= editorView();
	}
	ImGui::EndChild();

	ret |= dispatchActions();

	if (ret)
	{
		flash();
	}

	ImGui::End();
	ImGui::PopStyleColor();

	return ret;
}

void WorldEditor::setWorld(World* _world_)
{
	if (_world_ == m_currentWorld)
	{
		return;
	}

	if (!m_modifiedScenes.empty())
	{
		FRM_ASSERT(false); // \todo save dialogue
	}
	m_modifiedScenes.clear();

	m_currentWorld = _world_;
	m_currentScene = _world_->m_rootScene;
	m_currentNode  = _world_->m_rootScene->m_root.referent;

	m_nextNodeID = m_currentScene->findUniqueNodeID();
	m_nextComponentID = m_currentScene->findUniqueComponentID();

	flash();
}


// PRIVATE

WorldEditor* WorldEditor::s_current;

const char* WorldEditor::ActionTypeStr[(int)ActionType::_Count] =
{
	"Edit",
	"SelectNodeLocal",
	"SelectNodeGlobal",
	"SelectNodeParent",
	"SaveModifiedScene"
};

void WorldEditor::pushAction(ActionType _type, void* _context, void* _result)
{
	Action action = { _type, _context, _result };
	if (m_actionStack.empty() || action != m_actionStack.back())
	{
		m_actionStack.push_back(action);
	}

	switch(action.type)
	{
		default:
			break;
		case ActionType::SaveModifiedWorld:
		case ActionType::SaveModifiedScene:
			ImGui::OpenPopup("Save Modified");
			break;
	};
}

void WorldEditor::popAction()
{
	FRM_ASSERT(m_actionStack.size() > 1);
	Action action = m_actionStack.back();
	m_actionStack.pop_back();

	switch (action.type)
	{
		default:
		case ActionType::SelectNodeGlobal:
		case ActionType::SelectNodeLocal:
		{
			break;
		}
		case ActionType::SelectNodeParent:
		{
			((SceneNode*)action.context)->setParent((SceneNode*)action.result);
			break;
		}
	};
}

void WorldEditor::cancelAction()
{
	FRM_ASSERT(m_actionStack.size() > 1);
	Action action = m_actionStack.back();
	m_actionStack.pop_back();
}

bool WorldEditor::dispatchActions()
{
	bool ret = false;

	// Cancel current action if escape is pressed.
	if (m_actionStack.size() > 1 && Input::GetKeyboard()->wasPressed(Keyboard::Key_Escape))
	{
		cancelAction();
		AppSample::GetCurrent()->getWindow()->setCursorType(Window::CursorType_Arrow);
	}

	Action& action = m_actionStack.back();
	switch (action.type)
	{
		default:
		case ActionType::Edit:
		{
			break;
		}
		case ActionType::SelectNodeLocal:
		case ActionType::SelectNodeGlobal:
		case ActionType::SelectNodeParent:
		{
			//if (m_hoveredNode) // \todo need to modify cursor if hovered node is valid in this mode
			{
				AppSample* app = AppSample::GetCurrent();
				app->getWindow()->setCursorType(Window::CursorType_Cross);
				break;
			}
		}
		case ActionType::NewWorld:
		{			
			if (!m_modifiedScenes.empty())
			{
				pushAction(ActionType::SaveModifiedScene, m_modifiedScenes.begin()->second);
			}
			else if (m_worldModified)
			{
				pushAction(ActionType::SaveModifiedWorld, m_currentWorld);
			}
			else
			{
				m_currentWorld->shutdown();
				m_currentWorld->m_path = "";
				m_currentWorld->init();
				m_currentWorld->postInit();
				m_currentScene = m_currentWorld->m_rootScene;
				m_currentNode = m_currentScene->m_root.referent;
				m_nextNodeID = m_currentScene->findUniqueNodeID();
				m_nextComponentID = m_currentScene->findUniqueComponentID();
				popAction();
			}

			break;
		}
		case ActionType::LoadWorld:
		{
			if (!m_modifiedScenes.empty())
			{
				pushAction(ActionType::SaveModifiedScene, m_modifiedScenes.begin()->second);
			}
			else if (m_worldModified)
			{
				pushAction(ActionType::SaveModifiedWorld, m_currentWorld);
			}
			else
			{
				World* world = (World*)action.context;
				ret |= loadWorld(world);
				popAction();
			}

			break;
		}
		case ActionType::SaveWorld:
		{
			World* world = (World*)action.context;

			if (world->m_path.isEmpty())
			{
				if (!SelectRelativePath(world->m_path, "world"))
				{
					cancelAction();
					break;
				}
			}

			ret |= saveWorld(world);
			popAction();

			break;
		}
		case ActionType::SaveModifiedWorld:
		{
			World* world = (World*)action.context;
			String<64> choiceLabel("Save changes to world '%s'?", world->m_path.c_str());
			int choice = ImGui::ChoicePopupModal("Save Modified", choiceLabel.c_str(), { "Yes", "No", "Cancel"  });
			switch(choice)
			{
				default: 
					break;
				case 0: // yes
					popAction();
					pushAction(ActionType::SaveWorld, world);
					break;
				case 1: // no
					popAction();
					setWorldModified(world, false);
					break;
				case 2: // cancel
					popAction();
					cancelAction(); // Cancel action below this in the action stack.
					break;
			};

			break;
		}
		case ActionType::LoadScene:
		{
			Scene* scene = (Scene*)action.context;
			ret |= saveScene(scene);
			popAction();

			break;
		}
		case ActionType::SaveScene:
		{
			Scene* scene = (Scene*)action.context;

			if (scene->m_path.isEmpty())
			{
				PathStr path;
				if (!SelectRelativePath(path, "scene"))
				{
					cancelAction();
					break;
				}
				setScenePath(scene, path);	
			}

			ret |= saveScene(scene);
			popAction();

			break;
		}
		case ActionType::SaveModifiedScene:
		{
			Scene* scene = (Scene*)action.context;
			String<64> choiceLabel("Save changes to scene '%s'?", scene->m_path.c_str());
			int choice = ImGui::ChoicePopupModal("Save Modified", choiceLabel.c_str(), { "Yes", "No", "Cancel"  });
			switch(choice)
			{
				default: 
					break;
				case 0: // yes
					popAction();
					pushAction(ActionType::SaveScene, scene);
					break;
				case 1: // no
					popAction();
					setSceneModified(scene, false);
					break;
				case 2: // cancel
					popAction();
					cancelAction(); // Cancel action below this in the action stack.
					break;
			};

			break;
		}
	};

	return ret;
}

bool WorldEditor::loadWorld(World* _world_)
{
	FRM_STRICT_ASSERT(_world_);
	FRM_ASSERT(m_modifiedScenes.empty()); // \todo save existing scenes
	
	PathStr path;
	if (!SelectRelativePath(path, "world"))
	{
		return false;
	}

	Json json;
	if (!Json::Read(json, path.c_str()))
	{
		return false;
	}

	if (_world_ == m_currentWorld)
	{
		m_currentScene = nullptr;
		m_currentNode  = nullptr;
	}

	_world_->m_path = path;

	SerializerJson serializer(json, SerializerJson::Mode_Read);
	_world_->serialize(serializer);
	if (serializer.getError())
	{		
		FRM_LOG_ERR("Error serializing world: %s", serializer.getError());
		return false;
	}

	if (_world_ == m_currentWorld)
	{
		m_currentScene = _world_->m_rootScene;
		m_currentNode  = m_currentScene->m_root.referent;

		// \todo per scene
		m_nextNodeID = m_currentScene->findUniqueNodeID();
		m_nextComponentID = m_currentScene->findUniqueComponentID();
	}

	setWorldModified(_world_, false);

	return true;
}

bool WorldEditor::saveWorld(World* _world_)
{
	FRM_STRICT_ASSERT(_world_ && !_world_->m_path.isEmpty());

	Json json;
	SerializerJson serializer(json, SerializerJson::Mode_Write);
	_world_->serialize(serializer);
	if (serializer.getError())
	{
		FRM_LOG_ERR("Error serializing world: %s", serializer.getError());
		return false;
	}

	if (!Json::Write(json, _world_->m_path.c_str()))
	{
		return false;
	}

	if (_world_->m_rootScene->getPath().isEmpty())
	{
		setSceneModified(_world_->m_rootScene, false); // root scene was serialized inline
	}

	setWorldModified(_world_, false);

	return true;
}


void WorldEditor::setWorldModified(World* _world, bool _modified)
{
	m_worldModified = _modified;
}

bool WorldEditor::loadScene(Scene* _scene_)
{
	FRM_STRICT_ASSERT(_scene_);

	Json json;
	if (!Json::Read(json, _scene_->m_path.c_str()))
	{
		return false;
	}


	SerializerJson serializer(json, SerializerJson::Mode_Read);
	_scene_->serialize(serializer);

	if (serializer.getError())
	{
		FRM_LOG_ERR("Error serializing scene: %s", serializer.getError());
		return false;
	}

	setSceneModified(_scene_, false);

	return true;
}

bool WorldEditor::saveScene(Scene* _scene_)
{
	FRM_STRICT_ASSERT(_scene_ && !_scene_->getPath().isEmpty());

	Json json;
	SerializerJson serializer(json, SerializerJson::Mode_Write);
	_scene_->serialize(serializer);

	if (serializer.getError())
	{
		FRM_LOG_ERR("Error serializing scene: %s", serializer.getError());
		return false;
	}

	if (!Json::Write(json, _scene_->m_path.c_str()))
	{
		return false;
	}

	setSceneModified(_scene_, false);
	
	return true;
}

void WorldEditor::setScenePath(Scene* _scene_, const PathStr& _path)
{
	if (_scene_->m_path == _path)
	{
		return;
	}

	StringHash oldPathHash(_scene_->getPath().c_str());
	StringHash newPathHash(_path.c_str());
	auto it = m_modifiedScenes.find(oldPathHash);
	if (it != m_modifiedScenes.end())
	{
		FRM_ASSERT(_scene_ == it->second);
		m_modifiedScenes[newPathHash] = _scene_;
		m_modifiedScenes.erase(it);
	}

	_scene_->setPath(_path.c_str());
}

void WorldEditor::setSceneModified(Scene* _scene, bool _modified)
{
	// If the scene is inline mark the world as modified instead.
	if (isSceneInline(_scene))
	{
		setWorldModified(_scene->m_world, _modified);
		return;
	}

	// If the scene is not inline it *must* have a path.
	FRM_ASSERT(!_scene->getPath().isEmpty());

	const StringHash pathHash = StringHash(_scene->getPath().c_str());
	
	if (_modified)
	{
		auto exists = m_modifiedScenes.find(pathHash);
		if (exists != m_modifiedScenes.end())
		{
			// \todo In this case, 2 separate instances of the same scene were both modified and are therefore out-of-sync.
			// This needs to be handled in the editor by propagating changes between scene instances.
			FRM_ASSERT(exists->second == _scene);
		}

		m_modifiedScenes[pathHash] = _scene;
	}
	else
	{
		auto it = m_modifiedScenes.find(pathHash);
		if (it != m_modifiedScenes.end())
		{
			m_modifiedScenes.erase(it);
		}
	}
}

bool WorldEditor::worldMenu()
{
	bool ret = false;
	
	if (ImGui::BeginMenu("World"))
	{
		if (ImGui::MenuItem("New"))
		{
			pushAction(ActionType::NewWorld, m_currentWorld);
		}

		if (ImGui::MenuItem("Open.."))
		{
			pushAction(ActionType::LoadWorld, m_currentWorld);
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Save", nullptr, nullptr, m_currentWorld != nullptr))
		{
			pushAction(ActionType::SaveWorld, m_currentWorld);
		}

		if (ImGui::MenuItem("Save As..", nullptr, nullptr, m_currentWorld != nullptr))
		{
			if (SelectRelativePath(m_currentWorld->m_path, "world"))
			{
				pushAction(ActionType::SaveWorld, m_currentWorld);
			}
		}

		ImGui::EndMenu();
	}

	return ret;
}

bool WorldEditor::sceneMenu()
{
	bool ret = false;
	
	if (ImGui::BeginMenu("Scene", /*m_currentScene != nullptr*/false)) // \todo disabled, need child scene implementation
	{
		if (ImGui::MenuItem("Save"))
		{
			pushAction(ActionType::SaveScene, m_currentScene);
		}

		if (ImGui::MenuItem("Save As.."))
		{
			FRM_ASSERT(false); // - update the world scene instance map (remove this scene instance, re-insert with new path - DON'T change other instance paths, you're basically just dup'ing the scene.
			if (SelectRelativePath(m_currentScene->m_path, "scene"))
			{
				pushAction(ActionType::SaveScene, m_currentScene);
			}	
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Save All"))
		{
			while (!m_modifiedScenes.empty())
			{
				saveScene(m_modifiedScenes.begin()->second);
			}
		}

		ImGui::EndMenu();
	}

	return ret;
}

bool WorldEditor::viewMenu()
{
	bool ret = false;
	
	if (ImGui::BeginMenu("View"))
	{
		ImGui::MenuItem("Show node IDs", nullptr, &m_showNodeIDs);
		ImGui::MenuItem("Show 3D node labels", nullptr, &m_show3DNodeLabels);
		ImGui::MenuItem("Show transient nodes", nullptr, &m_showTransientNodes);

		ImGui::EndMenu();
	}

	return ret;
}

bool WorldEditor::hierarchyView(SceneNode* _rootNode_)
{
	// \todo
	// - Tables API for better columnation?
	// - Alternate row background for better readability, highlight background row.
	// - Color coding for node state (i.e. gray for inactive).
	// - Drag + drop to reparent.
	// - Show components.

	bool ret = false;

	eastl::fixed_vector<SceneNode*, 64> tstack;
	tstack.push_back(_rootNode_);
	while (!tstack.empty())
	{
		SceneNode* node = tstack.back();
		tstack.pop_back();

		// nullptr used as a sentinel to mark the end of a group of children
		if (!node)
		{
			ImGui::TreePop();
			continue;
		}

		bool isTransient = node->getFlag(SceneNode::Flag::Transient);
		if (!m_showTransientNodes && isTransient)
		{
			continue;
		}

		ImGui::PushID(node);

		auto nodeLabel = GetNodeLabel(node, m_showNodeIDs);
		if (node == m_currentNode)
		{
			nodeLabel.append(" " ICON_FA_CARET_LEFT);
		}

		const bool isActive = node->getFlag(SceneNode::Flag::Active);
	
		// Disable the default open on single-click behavior and pass in Selected flag according to our selection state.
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ((node == m_currentNode) ? ImGuiTreeNodeFlags_Selected : 0);
		bool selectNode = false;
		bool hoverNode = false;

		ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once); 

		if (node->m_children.empty() && !node->m_childScene)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			ImGui::TreeNodeEx(node, nodeFlags, nodeLabel.c_str());
			selectNode = ImGui::IsItemClicked() && !isTransient;
			hoverNode = ImGui::IsItemHovered();

			ImGui::SameLine(ImGui::GetWindowWidth() - 48.0f);
			if (ImGui::SmallButton(isActive ? ICON_FA_EYE : ICON_FA_EYE_SLASH))
			{
				node->setFlag(SceneNode::Flag::Active, !isActive);
			}
		}
		else
		{
			bool nodeOpen = ImGui::TreeNodeEx(node, nodeFlags, nodeLabel.c_str());
			selectNode = ImGui::IsItemClicked() && !isTransient;
			hoverNode = ImGui::IsItemHovered();

			ImGui::SameLine(ImGui::GetWindowWidth() - 48.0f);
			if (ImGui::SmallButton(isActive ? ICON_FA_EYE : ICON_FA_EYE_SLASH))
			{
				node->setFlag(SceneNode::Flag::Active, !isActive);
			}

			if (nodeOpen)
			{			
				tstack.push_back(nullptr); // force call to TreePop() later

				//for (LocalNodeReference& child : node->m_children)
				for (int i = (int)node->m_children.size() - 1; i >= 0; --i) // tstack is FILO so reverse iterator makes a more intuitive list
				{
					tstack.push_back(node->m_children[i].referent);
				}

				if (node->m_childScene)
				{
					tstack.push_back(node->m_childScene->m_root.referent);
				}
			}
		}


		if (selectNode)
		{
			Action& action = m_actionStack.back();
			switch (action.type)
			{
				default:
				{
					m_currentNode = node;
					m_currentScene = node->m_parentScene;
					break;
				}
				case ActionType::SelectNodeGlobal:
				{
					const Scene* scene = (Scene*)action.context;
					GlobalNodeReference globalReference = scene->findGlobal(node);
					if (globalReference.isValid())
					{
						*(GlobalNodeReference*)action.result = globalReference;
						popAction();
					}
					break;
				}
				case ActionType::SelectNodeLocal:
				{
					const Scene* scene = (Scene*)action.context;
					LocalNodeReference localReference = scene->findLocal(node);
					if (localReference.isValid())
					{
						*(LocalNodeReference*)action.result = localReference;
						popAction();
					}
					break;
				}
				case ActionType::SelectNodeParent:
				{
					LocalNodeReference localReference = ((SceneNode*)m_actionStack.back().context)->getParentScene()->findLocal(node);
					if (localReference.isValid())
					{
						action.result = node;
						popAction();
					}
					break;
				}
			};
		}

		if (hoverNode)
		{
			Im3d::Text(node->getPosition(), 1.0f, Im3d::Color_Gold, 0, node->getName());
			m_hoveredNode = node;
		}

		ImGui::PopID();

	}

	return ret;
}

bool WorldEditor::editorView()
{
	bool ret = false;

	if (m_currentScene)
	{
		ImGui::Text(ICON_FA_SITEMAP " Scene");
		ImGui::Separator();
		ret |= editScene(m_currentScene);
	}

	if (m_currentNode)
	{
		ImGui::Text(ICON_FA_CUBE " Node");
		ImGui::Separator();
		ret |= editNode(m_currentNode);
	}

	//ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNodeEx("DEBUG", ImGuiTreeNodeFlags_CollapsingHeader))
	{
		ImGui::Checkbox("Show Node Hierarchy", &m_debugShowNodeHierarchy);

		ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode("Debug Counters"))
		{
			size_t sceneCount     = 0;
			size_t nodeCount      = 0;
			size_t componentCount = 0;
			for (auto& sceneList : m_currentWorld->m_sceneInstances)
			{ 
				sceneCount += sceneList.second.size();
				
				for (Scene* scene : sceneList.second)
				{
					//FRM_ASSERT(scene->m_nodePool.getUsedCount() == scene->m_localNodeMap.size());
					nodeCount += scene->m_localNodeMap.size();
					componentCount += scene->m_componentMap.size();				
				}
			}
			ImGui::Text("# scenes     : %u", sceneCount);
			ImGui::Text("# nodes      : %u", nodeCount);
			ImGui::Text("# components : %u", componentCount);
	
			ImGui::TreePop();
		}

		if (m_debugShowNodeHierarchy)
		{
			Im3d::PushAlpha(0.5f);

			Im3d::PushColor(Im3d::Color_Gold);
			Im3d::PushSize(3.0f);
			m_currentWorld->m_rootScene->traverse([](SceneNode* _node)
				{
					Im3d::PushMatrix(_node->getWorld());
						Im3d::DrawAlignedBox(vec3(-0.05f), vec3(0.05f));
					Im3d::PopMatrix();
					return true;
				});
			Im3d::PopSize();
			Im3d::PopColor();

			Im3d::PushColor(Im3d::Color_White);
			Im3d::PushSize(6.0f);
			m_currentWorld->m_rootScene->traverse([](SceneNode* _node)
				{
					if (_node->m_parent.isValid())
					{
						Im3d::DrawArrow(_node->m_parent->getPosition(), _node->getPosition()); 
					}
					return true;
				});
			Im3d::PopSize();
			Im3d::PopColor();

			Im3d::PopAlpha();
		}

		if (ImGui::TreeNode("Action Stack"))
		{
			for (int i = (int)m_actionStack.size() - 1; i >= 0; --i)
			{
				ImGui::Text("%s %s", ActionTypeStr[(int)m_actionStack[i].type], (i == m_actionStack.size() - 1) ? ICON_FA_CARET_LEFT : "");
			}

			ImGui::TreePop();
		}
	}

	if (ret)
	{
		setSceneModified(m_currentScene, true);
	}

	return ret;
}

bool WorldEditor::editWorld(World* _world_)
{
	bool ret = false;

	return ret;
}

bool WorldEditor::editScene(Scene* _scene_)
{
	bool ret = false;

	// Path
	{
		if (ImGui::Button(ICON_FA_FOLDER " Path"))
		{
			PathStr path = _scene_->m_path;
			if (SelectRelativePath(path, "scene"))
			{
				FRM_ASSERT(false);
				setScenePath(_scene_, path);
				saveScene(_scene_);
			}
		}
		ImGui::SameLine();
		ImGui::Text("%s", _scene_->m_path.isEmpty() ? "--" : _scene_->m_path.c_str());
	}

	// Create/destroy node
	{
		//if (ImGui::Button(ICON_FA_PLUS " Create Node"))
		if (PrettyButton(ICON_FA_PLUS " Create Node", kCreateButtonColor))
		{
			beginCreateNode();
		}
		ret |= createNode();

		bool enableDestroy = m_currentNode && m_currentNode != _scene_->m_root.referent;
		bool enableDuplicate = m_currentNode != nullptr;
		{
			ImGui::SameLine();
			if (PrettyButton(ICON_FA_TIMES " Destroy Node", kDestroyButtonColor, enableDestroy))
			{
				_scene_->destroyNode(m_currentNode);
				m_currentNode = nullptr;
				ret = true;
			}
			
			ImGui::SameLine();
			if (PrettyButton(ICON_FA_CLONE " Duplicate Node", kDuplicateButtonColor, enableDuplicate))
			{
				SceneNode* newNode = duplicateNode(m_currentNode);
				if (newNode)
				{
					m_currentNode = newNode;
					ret = true;
				}
			}
		}
	}

	// \todo propagate changes to other scenes here?
	if (ret)
	{
		setSceneModified(_scene_, ret);
	}

	return ret;
}

bool WorldEditor::editNode(SceneNode* _node_)
{
	bool ret = false;
	
	ImGui::PushID(_node_);

	static ImGuiTextFilter filter;
	filter.Draw("Filter##WorldEditor::editNode");

	// Basic info
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	bool filterPassBasic = filter.PassFilter("BASIC");
	if ((filter.IsActive() && !filterPassBasic) || (filterPassBasic && ImGui::TreeNodeEx("BASIC", ImGuiTreeNodeFlags_CollapsingHeader)))
	{

		if (filterPassBasic || filter.PassFilter("Name"))
		{		
			if (m_showNodeIDs)
			{
				ImGui::AlignTextToFramePadding();
				ImGui::Text("[%04llX]", _node_->m_id);
				ImGui::SameLine();
			}
			ret |= ImGui::InputText("Name", (char*)_node_->m_name.begin(), _node_->m_name.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
		}

		if (filterPassBasic || filter.PassFilter("Active"))
		{
			bool isActive = _node_->m_flags.get(SceneNode::Flag::Active);
			if (ImGui::Checkbox("Active", &isActive))
			{
				_node_->m_flags.set(SceneNode::Flag::Active, isActive);
				ret = true;
			}
		}
	}

	// Transform
	//ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	bool filterPassTransform = filter.PassFilter("TRANSFORM");
	if ((filter.IsActive() && !filterPassTransform) || (filterPassTransform && ImGui::TreeNodeEx("TRANSFORM", ImGuiTreeNodeFlags_CollapsingHeader)))
	{
		// Modify the world space node transform, then transform back into parent space.
		mat4 parentWorld = identity;
		if (_node_->m_parent.referent)
		{
			parentWorld = _node_->m_parent->m_world;
		}
		else if (_node_->m_parentScene->m_parentNode)
		{
			parentWorld = _node_->m_parentScene->m_parentNode->m_world * parentWorld;
		}		
		mat4 childWorld  = parentWorld * _node_->m_local;
		if (Im3d::Gizmo("GizmoNodeLocal", (float*)&childWorld))
		{
			_node_->m_local = inverse(parentWorld) * childWorld;
			ret = true;
		}

		// \todo delta mode - input a delta rather than modifying the values directly
		vec3 position = GetTranslation(_node_->m_local);
		vec3 rotation = ToEulerXYZ(GetRotation(_node_->m_local));
		     rotation = vec3(Degrees(rotation.x), Degrees(rotation.y), Degrees(rotation.z));
		vec3 scale    = GetScale(_node_->m_local);

		// \todo reset buttons per position/rotation/scale

		if (filterPassTransform || filter.PassFilter("Position"))
		{
			if (ImGui::SmallButton(ICON_FA_DOT_CIRCLE_O "##ResetPosition"))
			{				
				SetTranslation(_node_->m_local, vec3(0.0f));
				ret = true;
			}
			ImGui::SameLine();

			if (ImGui::DragFloat3("Position", &position.x))
			{
				SetTranslation(_node_->m_local, position);
				ret = true;
			}
		}

		if (filterPassTransform || filter.PassFilter("Rotation"))
		{
			if (ImGui::SmallButton(ICON_FA_DOT_CIRCLE_O "##ResetRotation"))
			{				
				SetRotation(_node_->m_local, identity);
				ret = true;
			}
			ImGui::SameLine();

			if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f, -180.0f, 180.0f))
			{
				SetRotation(_node_->m_local, FromEulerXYZ(vec3(Radians(rotation.x), Radians(rotation.y), Radians(rotation.z))));
				ret = true;
			}
		}

		if (filterPassTransform || filter.PassFilter("Scale"))
		{
			if (ImGui::SmallButton(ICON_FA_DOT_CIRCLE_O "##ResetScale"))
			{				
				SetScale(_node_->m_local, vec3(1.0f));
				ret = true;
			}
			ImGui::SameLine();

			if (ImGui::DragFloat3("Scale", &scale.x, 1.0f, 1e-4f))
			{
				SetScale(_node_->m_local, scale);
				ret = true;
			}
		}
	}

	// Hierarchy
	//ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	bool filterPassHierarchy = filter.PassFilter("HIERARCHY");
	if ((filter.IsActive() && !filterPassHierarchy) || (filterPassHierarchy && ImGui::TreeNodeEx("HIERARCHY", ImGuiTreeNodeFlags_CollapsingHeader)))
	{
		// Parent
		
		if (filterPassHierarchy || filter.PassFilter("Parent"))
		{
			bool enableReparent = _node_ != m_currentScene->m_root.referent; // can't reparent root node
			{
				if (PrettyButton(ICON_FA_LIST " Parent", kNodeSelectButtonColor, enableReparent))
				{
					beginSelectNodeLocal();
				}
				LocalNodeReference newParent = selectNodeLocal(_node_->m_parent, _node_->m_parentScene);
				if (newParent.referent != _node_ && newParent != _node_->m_parent)
				{
					_node_->setParent(newParent.referent);
					ret = true;
				}

				ImGui::SameLine();
				if (PrettyButton(ICON_FA_EYEDROPPER "##Parent", kNodeSelectButtonColor, enableReparent))
				{
					pushAction(ActionType::SelectNodeParent, _node_);
				}

				ImGui::SameLine();
				if (_node_->m_parent.isValid())
				{
					if (TextLink(_node_->m_parent->getName()))
					{
						m_currentNode = _node_->m_parent.referent;
						flash();
					}
				}
				else
				{
					ImGui::Text("--");			
				}
			}
		}

		// Child scene

		/*if (filterPassHierarchy || filter.PassFilter("Child Scene"))
		{
			if (ImGui::Button(ICON_FA_FOLDER " Child Scene"))
			{
				PathStr path;
				if (SelectRelativePath(path, "scene"))
				{
					Json json;
					if (Json::Read(json, path.c_str()))
					{
						SerializerJson serializer(json, SerializerJson::Mode_Read);
						Scene* scene = FRM_NEW(Scene(_node_->m_parentScene->m_world));
						scene->m_path = path;

						if (scene->serialize(serializer))
						{
							_node_->setChildScene(scene);
							scene->init();
							scene->postInit();
							ret = true;
						}
						else
						{
							FRM_DELETE(scene);
						}
					}
				}
			}
			
			ImGui::SameLine();
			if (_node_->m_childScene)
			{
				ImGui::Text("'%s'", _node_->m_childScene->m_path.c_str());

				ImGui::SameLine();
				if (ImGui::SmallButton(ICON_FA_TIMES "##ChildScene"))
				{
					_node_->m_childScene->shutdown();
					FRM_DELETE(_node_->m_childScene);
					_node_->m_childScene = nullptr;
					ret = true;
				}
			}
			else
			{
				ImGui::Text("--");
			}
		}*/
	}

	// Components
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	bool filterPassComponents = filter.PassFilter("COMPONENTS");
	if ((filter.IsActive() && !filterPassComponents) || (filterPassComponents && ImGui::TreeNodeEx("COMPONENTS", ImGuiTreeNodeFlags_CollapsingHeader)))
	{
		if (PrettyButton(ICON_FA_PLUS " Create Component", kCreateComponentButtonColor))
		{
			beginCreateComponent();
		}
		ret |= createComponent(_node_);

		Component* toDelete = nullptr;
		for (auto& it : _node_->m_components)
		{
			Component* component = it.referent;

			const char* className = component->getClassRef()->getName();
			if (!filter.PassFilter(className)) // \todo separate filter for components?
			{
				continue;
			}

			ImGui::PushID(component);

			float cursorY = ImGui::GetCursorPosY();
			ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
			if (ImGui::TreeNodeEx(className, ImGuiTreeNodeFlags_AllowItemOverlap))
			{
				ret |= component->edit();
				ImGui::TreePop();
			}

			ImVec2 cursorRestore = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 64.0f, cursorY));
			if (ImGui::SmallButton(ICON_FA_TIMES))
			{
				toDelete = component;
			}
			ImGui::SetCursorPos(cursorRestore);

			ImGui::PopID(); // component
		}

		if (toDelete)
		{
			_node_->removeComponent(toDelete);
			ret = true;
		}
	}

	ImGui::PopID(); // node

	return ret;
}


void WorldEditor::beginSelectNodeGlobal()
{
	ImGui::OpenPopup("WorldEditor::selectNodeGlobal");
}

GlobalNodeReference WorldEditor::selectNodeGlobal(const GlobalNodeReference& _current, Scene* _scene)
{
	GlobalNodeReference ret = _current;

	if (!ImGui::BeginPopup("WorldEditor::selectNodeGlobal"))
	{
		return ret;
	}

	static ImGuiTextFilter filter;
	filter.Draw("Filter##WorldEditor::selectNodeGlobal");

	for (auto it : _scene->m_localNodeMap)
	{
		SceneNode* node = it.second;
		ImGui::PushID(node);

		if (node != _current.node && filter.PassFilter(node->m_name.begin(), node->m_name.end()))
		{
			if (ImGui::Selectable(GetNodeLabel(node, m_showNodeIDs).c_str()))
			{
				ret = GlobalNodeReference(0, it.first, node);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::PopID();
	}

	for (auto it : _scene->m_globalNodeMap)
	{
		SceneNode* node = it.second;
		ImGui::PushID(node);
		
		if (node != _current.node && filter.PassFilter(node->m_name.begin(), node->m_name.end()))
		{
			if (ImGui::Selectable(GetNodeLabel(node, m_showNodeIDs).c_str()))
			{
				ret = GlobalNodeReference(it.first.scene, it.first.local, node);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::PopID();
	}
	
	ImGui::EndPopup();

	return ret;
}

void WorldEditor::beginSelectNodeLocal()
{
	ImGui::OpenPopup("WorldEditor::selectNodeLocal");
}

LocalNodeReference WorldEditor::selectNodeLocal(const LocalNodeReference& _current, Scene* _scene)
{
	LocalNodeReference ret = _current;

	if (!ImGui::BeginPopup("WorldEditor::selectNodeLocal"))
	{
		return ret;
	}

	static ImGuiTextFilter filter;
	filter.Draw("Filter##WorldEditor::selectNodeLocal");

	for (auto it : _scene->m_localNodeMap)
	{
		SceneNode* node = it.second;
		ImGui::PushID(node);

		if (node != _current.referent && filter.PassFilter(node->m_name.begin(), node->m_name.end()))
		{
			if (ImGui::Selectable(GetNodeLabel(node, m_showNodeIDs).c_str()))
			{
				ret = LocalNodeReference(node);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::PopID();
	}
	
	ImGui::EndPopup();

	return ret;
}

static String<24> createNodeNameEdit; // Auto name for populating the text box during createNode().

void WorldEditor::beginCreateNode()
{
	ImGui::OpenPopup("WorldEditor::createNode");
	createNodeNameEdit.setf("Node_%04X", m_nextNodeID);
}

bool WorldEditor::createNode()
{
	bool ret = false;

	if (!m_currentScene || !ImGui::BeginPopup("WorldEditor::createNode"))
	{
		return ret;
	}

	// \todo
	// This should be more of a wizard with templates for lights, static or dynamic objects.

	bool createAndClose = false;

	createAndClose |= ImGui::InputText("Name", (char*)createNodeNameEdit.begin(), createNodeNameEdit.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
	createAndClose |= ImGui::Button("Create");

	if (createAndClose)
	{
		m_currentNode = m_currentScene->createNode(m_nextNodeID++, createNodeNameEdit.c_str()); // \todo select parent via popup?
		m_currentNode->init();
		m_currentNode->postInit();
		ret = true;
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();

	return ret;
}

SceneNode* WorldEditor::duplicateNode(const SceneNode* _node)
{
	Json json;
	SerializerJson serializerWrite(json, SerializerJson::Mode_Write);
	if (!const_cast<SceneNode*>(_node)->serialize(serializerWrite))
	{
		return nullptr;
	}

	const SceneID newNodeID = m_nextNodeID++;
	const String<24> newNodeName = _node->getName(); // \todo auto name
	SceneNode* newNode = _node->m_parentScene->createNode(newNodeID, newNodeName.c_str(), _node->m_parent.referent);
	
	SerializerJson serializerRead(json, SerializerJson::Mode_Read);
	bool ret = newNode->serialize(serializerRead);
	
	// New ID and name were overwritten by serialization, restore.
	newNode->m_id = newNodeID;
	newNode->m_name = newNodeName;
	
	newNode->m_children.clear(); // Any duplicated child references are invalid, remove them.

	Scene* parentScene = newNode->m_parentScene;
	for (LocalComponentReference& component : newNode->m_components)
	{
		parentScene->resolveReference(component);
		Component* newComponent = duplicateComponent(component.referent);
		if (!newComponent)
		{
			ret = false;
			break;
		}
		parentScene->addComponent(newComponent);
		component = LocalComponentReference(newComponent);
	}

	if (!ret)
	{
		_node->m_parentScene->destroyNode(newNode);
		return nullptr;
	}

	newNode->init();
	newNode->postInit();

	return newNode;
}

void WorldEditor::beginCreateComponent()
{
	ImGui::OpenPopup("WorldEditor::createComponent");
}

bool WorldEditor::createComponent(SceneNode* _node)
{
	bool ret = false;

	if (!m_currentScene || !ImGui::BeginPopup("WorldEditor::createComponent"))
	{
		return ret;
	}

	static ImGuiTextFilter filter;
	filter.Draw("Filter##WorldEditor::createComponent");

	Scene* scene = _node->getParentScene();
	FRM_STRICT_ASSERT(scene);

	for (int i = 0; i < Component::GetClassRefCount(); ++i)
	{
		const Component::ClassRef* cref = Component::GetClassRef(i);
		if (filter.PassFilter(cref->getName()))
		{
			if (ImGui::Selectable(cref->getName()))
			{
				Component* component = Component::Create(*cref, m_nextComponentID++);
				_node->addComponent(component);				
				ret = true;
								
				ImGui::CloseCurrentPopup();
			}
		}
	}

	ImGui::EndPopup();

	return ret;
}

Component* WorldEditor::duplicateComponent(const Component* _component)
{
	Json json;
	SerializerJson serializerWrite(json, SerializerJson::Mode_Write);
	if (!const_cast<Component*>(_component)->serialize(serializerWrite))
	{
		return nullptr;
	}

	const SceneID newComponentID = m_nextComponentID++; 
	Component* newComponent = Component::Create(*_component->getClassRef(), newComponentID);

	SerializerJson serializerRead(json, SerializerJson::Mode_Read);
	if (!newComponent->serialize(serializerRead))
	{
		Component::Destroy(newComponent);
		return nullptr;
	}

	newComponent->m_id = newComponentID; // New ID was overwritten by serialization, restore.

	return newComponent;
}

bool WorldEditor::isSceneInline(const Scene* _scene)
{
	return _scene == _scene->m_world->m_rootScene && _scene->m_path.isEmpty();
}

} // namespace frm