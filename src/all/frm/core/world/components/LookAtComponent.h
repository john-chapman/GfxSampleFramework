#pragma once

#include "Component.h"

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// LookAtComponent
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(LookAtComponent)
{
public:

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);
	        
	void        setTargetNode(GlobalNodeReference& _nodeRef);

private:

	GlobalNodeReference m_targetNode; // If set, world space position is the target.
	vec3                m_offset;     // Offset from the node position, or target if node isn't set.

	static void OnNodeShutdown(SceneNode* _node, void* _component);

	bool postInitImpl() override;
	void shutdownImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
};

} // namespace frm
