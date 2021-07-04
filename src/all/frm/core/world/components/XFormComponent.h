#pragma once

#include "Component.h"

#include <frm/core/Factory.h>
#include <frm/core/Serializable.h>

#include <EASTL/fixed_vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// XForm
// Base class for XForm implementations. XForms are simple world space modifiers
// which can be used to do simple animation and behavior. See XFormComponent.
////////////////////////////////////////////////////////////////////////////////
class XForm: public Factory<XForm>
{
public:

	// Reset initial state.
	virtual void reset() {}

	// Set state to initial state + current state.
	virtual void relativeReset() {}

	// Reverse behavior.
	virtual void reverse() {}

	// Apply behavior to _node_.
	virtual void apply(float _dt, SceneNode* _node_) = 0;

	// Edit, return true if changed.
	virtual bool edit() { return false; }

	// Serialize, return false if error.
	virtual bool serialize(Serializer& _serializer_) { return true; }

	// \todo Callback system below basically manages a map of unique instances. Could generalize this pattern as per Factory.
	typedef void (Callback)(XForm* _xform_, SceneNode* _node_);
	struct CallbackReference
	{
		Callback*   m_callback = nullptr;
		const char* m_name     = "";
		StringHash  m_nameHash = StringHash::kInvalidHash;

		CallbackReference(const char* _name, Callback* _callback);
	};
	static int                      GetCallbackCount();
	static const CallbackReference* GetCallback(int _i);
	static const CallbackReference* FindCallback(frm::StringHash _nameHash);
	static const CallbackReference* FindCallback(Callback* _callback);
	static bool                     EditCallback(const CallbackReference*& _callback_, const char* _name);
	static bool                     SerializeCallback(Serializer& _serializer_, const CallbackReference*& _callback_, const char* _name);
	
private:

	friend class XFormComponent;
};

#define FRM_XFORM_DECLARE(_class) \
	class _class: public XForm, public Serializable<_class>
#define FRM_XFORM_DEFINE(_class, _version) \
	FRM_SERIALIZABLE_DEFINE(_class, _version); \
	FRM_FACTORY_REGISTER_DEFAULT(XForm, _class); \
	FRM_FORCE_LINK_REF(_class)

#define FRM_XFORM_REGISTER_CALLBACK(_name, _callback) \
	static XForm::CallbackReference FRM_UNIQUE_NAME(_XFormCallbackReference)(_name, _callback)

////////////////////////////////////////////////////////////////////////////////
// XFormComponent
// Manage a stack of xforms which are applied to the parent node in order.
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(XFormComponent)
{
public:

	static void  Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);

	virtual void reset() override;

	void         addXForm(XForm* _xform_);
	void         removeXForm(XForm*& _xform_);

	virtual      ~XFormComponent();

private:

	eastl::fixed_vector<XForm*, 1> m_xforms;

	void         update(float _dt);
	bool         editImpl() override;
	bool         serializeImpl(Serializer& _serializer_) override;
	bool         isStatic() override { return false; }

	void         moveXForm(int _from, int _to);
};

} // namespace frm
