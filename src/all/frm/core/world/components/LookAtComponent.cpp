#include "LookAtComponent.h"

#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/world/WorldEditor.h>

#include <imgui/imgui.h>

namespace frm {

FRM_COMPONENT_DEFINE(LookAtComponent, 0);

// PUBLIC

void LookAtComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("LookAtComponent::Update");

	if (_phase != World::UpdatePhase::PrePhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		LookAtComponent* component = (LookAtComponent*)*_from;

		vec3 target = component->m_offset;
		if (component->m_targetNode.isResolved())
		{
			target += GetTranslation(component->m_targetNode->getWorld());
		}

		SceneNode* node = component->m_parentNode;
		const vec3 origin = GetTranslation(node->getWorld());
		
		mat4 m = AlignY(Normalize(target - origin), vec3(0,0,1));
		SetTranslation(m, origin);
		component->m_parentNode->setWorld(m);
	}
}

void LookAtComponent::setTargetNode(GlobalNodeReference& _nodeRef)
{
	if (m_targetNode.isResolved())
	{
		m_targetNode->unregisterCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
	}

	m_targetNode = _nodeRef;
	if (m_targetNode.isResolved())
	{
		m_targetNode->registerCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
	}
}

// PRIVATE

void LookAtComponent::OnNodeShutdown(SceneNode* _node, void* _component)
{
	FRM_STRICT_ASSERT(_node);
	FRM_STRICT_ASSERT(_component);

	GlobalNodeReference& nodeRef = ((LookAtComponent*)_component)->m_targetNode;
	FRM_ASSERT(nodeRef.node == _node);
	nodeRef.node->unregisterCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, _component);
	nodeRef = GlobalNodeReference();
}

bool LookAtComponent::postInitImpl()
{
	FRM_ASSERT(m_parentNode);

	Scene* scene = m_parentNode->getParentScene();
	FRM_ASSERT(scene);

	bool ret = true;

	if (m_targetNode.isValid())
	{
		scene->resolveReference(m_targetNode);
	}

	if (m_targetNode.isResolved())
	{
		m_targetNode->registerCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
	}
	
	return ret;
}

void LookAtComponent::shutdownImpl()
{
	if (m_targetNode.isResolved())
	{
		m_targetNode->unregisterCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
	}
}

bool LookAtComponent::editImpl()
{
	bool ret = false;

	WorldEditor* worldEditor = WorldEditor::GetCurrent();
	
	if (ImGui::Button("Target Node"))
	{
		worldEditor->beginSelectNode();
	}
	GlobalNodeReference newNodeRef = worldEditor->selectNode(m_targetNode, m_parentNode->getParentScene());
	if (newNodeRef != m_targetNode)
	{
		setTargetNode(newNodeRef);
		ret = true;
	}

	if (m_targetNode.isResolved())
	{
		ImGui::SameLine();
		ImGui::Text(m_targetNode->getName());
	}

	ImGui::Spacing();
	ret |= ImGui::DragFloat3("Offset", &m_offset.x);

	return ret;
}

bool LookAtComponent::serializeImpl(Serializer& _serializer_)
{	
	bool ret = SerializeAndValidateClass(_serializer_);
	ret &= m_targetNode.serialize(_serializer_, "m_targetNode");
	ret &= Serialize(_serializer_, m_offset, "m_offset");
	return ret;	
}

} // namespace frm
