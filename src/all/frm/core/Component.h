#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/Factory.h>
#include <frm/core/FileSystem.h>

#include <EASTL/fixed_vector.h>

namespace frm {

class Node;

////////////////////////////////////////////////////////////////////////////////
// Component
// Base class/factory for Components.
////////////////////////////////////////////////////////////////////////////////
class Component: public Factory<Component>
{
public:
	
	virtual bool init()                                     { return true; }
	static  bool Init(Component* _component_)               { return _component_->init(); }

	virtual void shutdown()                                 {}
	static  void Shutdown(Component* _component_)           { _component_->shutdown(); }

	virtual void update(float _dt)                          {}
	static  void Update(Component* _component_, float _dt)  { _component_->update(_dt); }

	virtual bool edit()                                     { return false; }
	static  bool Edit(Component* _component_)               { return _component_->edit(); }

	const char*  getName() const                            { return getClassRef()->getName(); }	
	Node*        getNode() const                            { return m_node; }
	void         setNode(Node* _node)                       { m_node = _node; }

	virtual bool serialize(Serializer& _serializer_) = 0;
	friend bool Serialize(Serializer& _serializer_, Component& _component_)
	{
		return _component_.serialize(_serializer_);
	}

protected:
	Component(): m_node(nullptr) {}

	Node* m_node;

}; // class Component

////////////////////////////////////////////////////////////////////////////////
// Component_BasicRenderable
// Note that by design the component is *passive* - it's agnostic wrt the 
// renderer implementation. To avoid having the renderer traverse the scene graph
// every frame we cache all instances of the component in a static array.
//
// \todo 
// - Because all framework resources implicitly call Use() on creation, we're 
//   forced to store and serialize the paths separately in order to be able to 
//   defer the loading.
// - Also cache the world space transform/AABB to avoid dereferencing the node
//   ptr?
////////////////////////////////////////////////////////////////////////////////
struct Component_BasicRenderable: public Component
{
	vec4                                   m_colorAlpha  = vec4(1.0f);
	bool                                   m_castShadows = true;
	mat4                                   m_prevWorld   = identity;
	Mesh*                                  m_mesh        = nullptr;
	PathStr                                m_meshPath    = "";
	eastl::fixed_vector<BasicMaterial*, 1> m_materials;      // per submesh
	eastl::fixed_vector<PathStr, 1>   m_materialPaths;  //     "

	static eastl::vector<Component_BasicRenderable*> s_instances;

	virtual bool init() override;
	virtual void shutdown() override;
	virtual void update(float _dt) override;
	virtual bool edit() override;
	virtual bool serialize(Serializer& _serializer_) override;
};

////////////////////////////////////////////////////////////////////////////////
// Component_BasicLight
////////////////////////////////////////////////////////////////////////////////
struct Component_BasicLight: public Component
{
	enum Type_
	{
		Type_Direct,
		Type_Point,
		Type_Spot,

		Type_Count
	};
	typedef int Type;

	Type m_type                   = Type_Direct;
	vec4 m_colorBrightness        = vec4(1.0f);
	bool m_castShadows            = false;
	vec2 m_linearAttenuation      = vec2(0.0f); // start, stop in meters
	vec2 m_radialAttenuation      = vec2(0.0f); // start, stop in degrees

	static eastl::vector<Component_BasicLight*> s_instances;

	virtual bool init() override;
	virtual void shutdown() override;
	virtual void update(float _dt) override;
	virtual bool edit() override;
	virtual bool serialize(Serializer& _serializer_) override;
};

} // namespace frm
