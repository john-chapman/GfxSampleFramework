#include "Light.h"

#include <apt/Serializer.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;
using namespace apt;

// PUBLIC

Light::Light(Node* _parent)
{
	m_parent = _parent;
}

Light::~Light()
{
}

bool frm::Serialize(Serializer& _serializer_, Light& _light_)
{
 // note that the parent node doesn't get written here - the scene serializes the Light params *within* a node so it's not required
	//_serializer_.value("Up",          m_up);
	
	return true;
}

void Light::edit()
{
	ImGui::PushID(this);
	Im3d::PushId(this);


	Im3d::PopId();
	ImGui::PopID();
}
