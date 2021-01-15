#include "TextComponent.h"

#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/world/World.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

FRM_COMPONENT_DEFINE(TextComponent, 0);

// PUBLIC

void TextComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("TextComponent::Update");

	if (_phase != World::UpdatePhase::PreRender)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		TextComponent* component = (TextComponent*)*_from;

		vec3 position = component->getParentNode()->getPosition() + component->m_offset;
		Im3d::Color color = Im3d::Color(component->m_colorAlpha);
		Im3d::Text(position, component->m_size, color, Im3d::TextFlags_Default, component->m_text.c_str());
	}
}

// PRIVATE

bool TextComponent::editImpl()
{
	bool ret = false;

	ret |= ImGui::InputText("Text", (char*)m_text.begin(), m_text.getCapacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

	ret |= ImGui::SliderFloat("Size", &m_size, 1e-2f, 10.0f);

	ret |= ImGui::ColorEdit4("Color/Alpha", &m_colorAlpha.x);

	ret |= ImGui::DragFloat3("Offset", &m_offset.x, 0.05f);

	return ret;
}

bool TextComponent::serializeImpl(Serializer& _serializer_)
{	
	bool ret = SerializeAndValidateClass(_serializer_);
	ret &= Serialize(_serializer_, m_text,       "m_text");
	ret &= Serialize(_serializer_, m_size,       "m_size");
	ret &= Serialize(_serializer_, m_colorAlpha, "m_colorAlpha");
	ret &= Serialize(_serializer_, m_offset,     "m_offset");
	return ret;	
}

} // namespace frm
