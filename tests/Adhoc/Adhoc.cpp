#include "Adhoc.h"

#include <frm/core/frm.h>
#include <frm/core/log.h>

#include <frm/core/Json.h>
#include <frm/core/Properties.h>

using namespace frm;

static Adhoc s_inst;

Adhoc::Adhoc()
	: AppBase("Adhoc") 
{
}

Adhoc::~Adhoc()
{
}

void TraverseGroup(Json& json, int depth = 0)
{
	while (json.next())
	{
		auto name = json.getName();
		FRM_LOG("%*s%s", depth * 4, "", name);
		auto type = json.getType();
		if (type == Json::ValueType_Object)
		{
			if (json.enterObject())
			{
				TraverseGroup(json, depth + 1);
				json.leaveObject();
			}
		}
	}
}

bool Adhoc::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	return true;
}

void Adhoc::shutdown()
{
	
	AppBase::shutdown();
}

bool Adhoc::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	static ImGuiTextFilter filter;
	filter.Draw();
	filter.Build();

	Properties::GetCurrent()->edit(filter.InputBuf);

	if (ImGui::TreeNode("Display"))
	{
		Properties::GetCurrent()->display(filter.InputBuf);
		ImGui::TreePop();
	}

	return true;
}

void Adhoc::draw()
{
 // code here

	AppBase::draw();
}
