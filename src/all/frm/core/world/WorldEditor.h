#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/StringHash.h>
#include <frm/core/world/World.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/map.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// WorldEditor
//
// \todo
// - Push/Pop ID based on _scene_/_node_ args to edit() is currently disabled as
//   it provides better UX (tree nodes stay open when switching selections).
// - Manage gizmos more coherently - avoid having multiple gizmos at once?
// - Store m_currentNode as a ring buffer, implement back/forward navigation?
// - Color coding for hierarchy/component/basic editor headings.
// - Layers:
//    - Requires editor-specific data per world (see below)
//    - Group nodes into layers, activate/deactivate them per layer.
// - Persistent editor state:
//    - Cache in a properties file alongside the world file.
//    - Store ImGui state?
////////////////////////////////////////////////////////////////////////////////
class WorldEditor
{
public:
	
	static WorldEditor* GetCurrent()                             { return s_current; }
	static void         SetCurrent(WorldEditor* _worldEditor)    { s_current = _worldEditor; }

	WorldEditor();
    ~WorldEditor();

	bool edit();

	void setWorld(World* _world_);

	void beginSelectNode() { beginSelectNodeGlobal(); }
	GlobalNodeReference selectNode(const GlobalNodeReference& _current, Scene* _scene) { return selectNodeGlobal( _current, _scene); }

private:

	using SceneMap = eastl::map<StringHash, Scene*>;

	enum class ActionType
	{
		Edit,

		SelectNodeLocal,
		SelectNodeGlobal,
		SelectNodeParent,

		NewWorld,
		LoadWorld,
		SaveWorld,
		SaveModifiedWorld,
		LoadScene,
		SaveScene,
		SaveModifiedScene,

		_Count
	};
	static const char* ActionTypeStr[(int)ActionType::_Count];

	struct Action
	{
		ActionType type;
		void*      context;
		void*      result;

		bool operator==(const Action& _rhs) { return type == _rhs.type && context == _rhs.context && result == _rhs.result; }
		bool operator!=(const Action& _rhs) { return !(*this == _rhs); }
	};

	eastl::fixed_vector<Action, 3> m_actionStack;
	void pushAction(ActionType _type, void* _context = nullptr, void* _result = nullptr);
	void popAction();
	void cancelAction();
	bool dispatchActions();


	static WorldEditor*  s_current;

	bool                 m_showNodeIDs             = true;
	bool                 m_show3DNodeLabels        = false;
	bool                 m_showTransientNodes      = false;

	World*               m_currentWorld            = nullptr;
	Scene*               m_currentScene            = nullptr;
	SceneNode*           m_currentNode             = nullptr;
	SceneNode*           m_hoveredNode             = nullptr;
	SceneMap             m_modifiedScenes;
	bool                 m_worldModified           = false;
	SceneID              m_nextNodeID              = 0u; // \todo per SCENE
	SceneID              m_nextComponentID         = 0u; // \todo per SCENE
	float                m_hierarchyViewHeight     = 256.0f;
	float                m_flash                   = 0.0f;
	bool                 m_debugShowNodeHierarchy  = true;

	bool loadWorld(World* _world_);
	bool saveWorld(World* _world_);
	void setWorldModified(World* _world, bool _modified);

	bool loadScene(Scene* _scene_);
	bool saveScene(Scene* _scene_);
	void setScenePath(Scene* _scene_, const PathStr& _path);
	void setSceneModified(Scene* _scene, bool _modified);

	bool worldMenu();
	bool sceneMenu();
	bool viewMenu();

	bool hierarchyView(SceneNode* _rootNode_);
	bool editorView();

	bool editWorld(World* _world_);
	bool editScene(Scene* _scene_);
	bool editNode(SceneNode* _node_);

	void beginSelectNodeGlobal();
	GlobalNodeReference selectNodeGlobal(const GlobalNodeReference& _current, Scene* _scene);
	void beginSelectNodeLocal();
	LocalNodeReference selectNodeLocal(const LocalNodeReference& _current, Scene* _scene);

	void beginCreateNode();
	bool createNode();
	SceneNode* duplicateNode(const SceneNode* _node);

	void beginCreateComponent();
	bool createComponent(SceneNode* _node_);
	Component* duplicateComponent(const Component* _component);

	bool isSceneInline(const Scene* _scene);

	void flash() { m_flash = 1.0f; }
};

} // namespace frm