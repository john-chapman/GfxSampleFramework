#include "VirtualWindowTest.h"

#include <frm/core/frm.h>

#include <imgui/imgui.h>
#include <imgui/imgui_ext.h>

using namespace frm;

static VirtualWindowTest s_inst;

VirtualWindowTest::VirtualWindowTest()
	: AppBase("VirtualWindow") 
{
}

VirtualWindowTest::~VirtualWindowTest()
{
}

bool VirtualWindowTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

 // code here

	return true;
}

void VirtualWindowTest::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool VirtualWindowTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}


	ImGui::InputInt2("SizeW", &m_sizeW.x);
	ImGui::DragFloat2("SizeV", &m_sizeV.x);
	ImGui::Checkbox("Scroll Bars", &m_scrollBars);

	ImGui::VirtualWindow::SetNextRegion(vec2(-1.0f), vec2(1.0f), ImGuiCond_Once);
	ImGui::VirtualWindow::SetNextRegionExtents(m_sizeV * -0.5f, m_sizeV * 0.5f, ImGuiCond_Always);
	if (ImGui::VirtualWindow::Begin(ImGui::GetID("Virtual Window"), ImVec2((float)m_sizeW.x, (float)m_sizeW.y), 0
		| ImGui::VirtualWindow::Flags_Default 
		| ImGui::VirtualWindow::Flags_PanZoom
		| (m_scrollBars ? ImGui::VirtualWindow::Flags_ScrollBars : 0)
		)) 
	{
		ImGui::VirtualWindow::Grid(ImVec2(8, 8), ImVec2(0.01f, 0.01f), ImVec2(10, 10));

		auto& drawList = *ImGui::GetWindowDrawList();
		drawList.AddRectFilledMultiColor(
			ImGui::VirtualWindow::ToWindow(vec2(-0.5f)),
			ImGui::VirtualWindow::ToWindow(vec2(0.5f)),
			IM_COL32_BLACK,
			IM_COL32_RED,
			IM_COL32_YELLOW,
			IM_COL32_GREEN
			);

		drawList.AddRect(
			ImGui::VirtualWindow::ToWindow(m_sizeV * -0.5f), 
			ImGui::VirtualWindow::ToWindow(m_sizeV *  0.5f),
			IM_COL32_MAGENTA
			);
		drawList.AddRect(
			ImGui::VirtualWindow::ToWindow(m_sizeV * -0.25f), 
			ImGui::VirtualWindow::ToWindow(m_sizeV *  0.25f),
			IM_COL32_YELLOW
			);

		ImGui::VirtualWindow::End();
	}

	return true;
}

void VirtualWindowTest::draw()
{
 // code here

	AppBase::draw();
}
