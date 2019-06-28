#pragma once

#include <frm/core/def.h>
#include <frm/core/math.h>

#include <apt/Factory.h>
#include <apt/FileSystem.h>

#include <EASTL/fixed_vector.h>

namespace frm {

class Node;

////////////////////////////////////////////////////////////////////////////////
// Component
// Base class/factory for Components.
////////////////////////////////////////////////////////////////////////////////
class Component: public apt::Factory<Component>
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

	virtual bool serialize(apt::Serializer& _serializer_) = 0;
	friend bool Serialize(apt::Serializer& _serializer_, Component& _component_)
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
	apt::PathStr                           m_path        = "";
	Mesh*                                  m_mesh        = nullptr;
	apt::PathStr                           m_meshPath    = "";
	bool                                   m_castShadows = true;
	eastl::fixed_vector<BasicMaterial*, 1> m_materials;      // per submesh
	eastl::fixed_vector<apt::PathStr, 1>   m_materialPaths;  //     "

	static eastl::vector<Component_BasicRenderable*> s_instances;

	virtual bool init() override;
	virtual void shutdown() override;
	virtual void update(float _dt) override;
	virtual bool edit() override;
	virtual bool serialize(apt::Serializer& _serializer_) override;
};

} // namespace frm
