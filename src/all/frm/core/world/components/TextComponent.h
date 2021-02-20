#pragma once

#include "Component.h"

#include <frm/core/String.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// TextComponent
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(TextComponent)
{
public:

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);

private:

	String<64> m_text       = "TextComponent";
	float      m_size       = 1.0f;
	vec4       m_colorAlpha = vec4(1.0f);
	vec3       m_offset     = vec3(0.0f);

	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
	bool isStatic() override { return true; }
};

} // namespace frm
