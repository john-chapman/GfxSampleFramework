#pragma once

#include <frm/core/frm.h>
#include <frm/core/Camera.h>
#include <frm/core/Factory.h>
#include <frm/core/Serializable.h>
#include <frm/core/world/World.h>

#include <eastl/map.h>
#include <eastl/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Component
// Base class/factory for Components. Components should implement an an Update()
// function which is called for a range of active components during each update
// phase (see World.h).
//
// \todo
// - Static accessor for active components (see BasicRenderableComponent).
////////////////////////////////////////////////////////////////////////////////
class Component: public Factory<Component>
{
public:
	
	using ComponentList = eastl::vector<Component*>;

	typedef void (UpdateFunc)(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase); 

	struct RegisterUpdateFunc
	{
		RegisterUpdateFunc(UpdateFunc* _func, const char* _className);
		~RegisterUpdateFunc();

		StringHash classNameHash;
	};

	static Component* Create(const ClassRef& _cref, SceneID _id = 0u);
	static Component* Create(StringHash _name, SceneID _id = 0u);

	// Update active components for every class.
	static void  Update(float _dt, World::UpdatePhase _phase);

	template <typename tComponent, typename tUpdateLambda>
	static void DefaultUpdate(tComponent** _from, tComponent** _to, float _dt, World::UpdatePhase _phase, World::UpdatePhase _phaseMask, tUpdateLambda&& _lambda)
	{
		if (_phase != _phaseMask)
		{
			return;
		}
		
		while (_from != _to)
		{
			tComponent* component = *_from;
			++_from;

			_lambda(component, _dt);
		}
	}

	// Clear active lists for all components.
	static void  ClearActiveComponents();

	// Get the active list for a given class.
	static const ComponentList& GetActiveComponents(StringHash _classNameHash);
	

	bool         edit();
	bool         serialize(Serializer& _serializer_);
	
	SceneNode*   getParentNode() const                        { return m_parentNode; }
	void         setParentNode(SceneNode* _node)              { m_parentNode = _node; }

	// Add this component instance to the per-class active list.
	void         setActive(bool _active);

	const char*  getName() const                              { return getClassRef()->getName(); }
	SceneID      getID() const                                { return m_id; }
	World::State getState() const                             { return m_state; }

protected:

	SceneNode* m_parentNode = nullptr;

	// Deriving class implementations for init(), postInit(), shutdown().
	virtual bool initImpl()                                   { return true; }
	virtual bool postInitImpl()                               { return true; }
	virtual void shutdownImpl()                               {}
	virtual bool editImpl()                                   { return false; }
	virtual bool serializeImpl(Serializer& _serializer_)      { return true; }

private:

	SceneID      m_id = 0u;
	World::State m_state = World::State::Shutdown;

	static eastl::map<StringHash, ComponentList> s_activeComponents;
	static eastl::map<StringHash, UpdateFunc*>*  s_updateFuncs;

	bool init()                                               { FRM_ASSERT(m_state == World::State::Shutdown); m_state = World::State::Init; return initImpl(); }
	bool postInit()                                           { FRM_ASSERT(m_state == World::State::Init); m_state = World::State::PostInit; return postInitImpl(); }
	void shutdown()                                           { FRM_ASSERT(m_state == World::State::PostInit); m_state = World::State::Shutdown; shutdownImpl(); }

	friend class Scene;
	friend class SceneNode;
	friend class WorldEditor;

}; // class Component

#define FRM_COMPONENT_DECLARE(_class) \
	class _class: public frm::Component, public frm::Serializable<_class>

#define FRM_COMPONENT_DECLARE_DERIVED(_class, _baseComponent) \
	class _class: public _baseComponent

#define FRM_COMPONENT_DEFINE(_class, _version) \
	FRM_SERIALIZABLE_DEFINE(_class, _version); \
	FRM_FACTORY_REGISTER_DEFAULT(frm::Component, _class); \
	static frm::Component::RegisterUpdateFunc s_ ## _class ## ComponentUpdateFunc(&_class::Update, #_class); \
	FRM_FORCE_LINK_REF(_class)

} // namespace frm
