#include "imgui.h"

void ImGui::Value(const char* prefix, const frm::vec2& v)
{
	Text("%s: %+0.5f, %+0.5f", prefix, v.x, v.y);
}
void ImGui::Value(const char* prefix, const frm::vec3& v)
{
	Text("%s: %+0.5f, %+0.5f, %+0.5f", prefix, v.x, v.y, v.z);
}
void ImGui::Value(const char* prefix, const frm::vec4& v)
{
	Text("%s: %+0.5f, %+0.5f, %+0.5f, %+0.5f", prefix, v.x, v.y, v.z, v.w);
}
void ImGui::Value(const char* prefix, const frm::mat3& v)
{
	Text("%s:\n"
		"   %+0.5f, %+0.5f, %+0.5f,\n"
		"   %+0.5f, %+0.5f, %+0.5f,\n"
		"   %+0.5f, %+0.5f, %+0.5f,",
		prefix,
		v[0][0], v[1][0], v[2][0],
		v[0][1], v[1][1], v[2][1],
		v[0][2], v[1][2], v[2][2]
		);
}
void ImGui::Value(const char* prefix, const frm::mat4& v)
{
	Text("%s:\n"
		"   %+0.5f, %+0.5f, %+0.5f, %+0.5f,\n"
		"   %+0.5f, %+0.5f, %+0.5f, %+0.5f,\n"
		"   %+0.5f, %+0.5f, %+0.5f, %+0.5f,\n"
		"   %+0.5f, %+0.5f, %+0.5f, %+0.5f",
		prefix,
		v[0][0], v[1][0], v[2][0], v[3][0],
		v[0][1], v[1][1], v[2][1], v[3][1],
		v[0][2], v[1][2], v[2][2], v[3][2],
		v[0][3], v[1][3], v[2][3], v[3][3]
		);
}

bool ImGui::ComboInt(const char* label, int* current_value, const char* items_separated_by_zeros, const int* item_values, int value_count)
{
	int select = 0;
	for (; select < value_count; ++select) {
		if (item_values[select] == *current_value) {
			break;
		}
	}
	if_likely (select < value_count) {
		if (ImGui::Combo(label, &select, items_separated_by_zeros)) {
			*current_value = item_values[select];
			return true;
		}
	} else {
		item_values[0];
		ImGui::Text("ImGui::ComboInt; '%d' not a valid value", *current_value);
	}
	return false;
}

bool ImGui::ComboFloat(const char* label, float* current_value, const char* items_separated_by_zeros, const float* item_values, int value_count)
{
	int select = 0;
	for (; select < value_count; ++select) {
		if (item_values[select] == *current_value) {
			break;
		}
	}
	if_likely (select < value_count) {
		if (ImGui::Combo(label, &select, items_separated_by_zeros)) {
			*current_value = item_values[select];
			return true;
		}
	} else {
		item_values[0];
		ImGui::Text("ImGui::ComboInt; '%d' not a valid value", *current_value);
	}
	return false;
}

bool ImGui::BeginInvisible(const char* name, frm::vec2 origin, frm::vec2 size, bool* p_open, int flags)
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
	ImGui::SetNextWindowPos(origin);
	ImGui::SetNextWindowSize(size);
	return ImGui::Begin(name, p_open,
		(ImGuiWindowFlags)flags |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoBringToFrontOnFocus
	);
}
void ImGui::EndInvisible()
{
	ImGui::End();
	ImGui::PopStyleColor(1);
}