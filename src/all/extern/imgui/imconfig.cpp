#include "imgui.h"

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