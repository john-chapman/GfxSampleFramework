#include "LuaScriptTest.h"

#include <frm/core/frm.h>
#include <frm/core/log.h>
#include <frm/core/LuaScript.h>
#include <frm/core/StringHash.h>

#include <imgui/imgui.h>

using namespace frm;

static LuaScriptTest s_inst;

LuaScriptTest::LuaScriptTest()
	: AppBase("LuaScript") 
{
}

LuaScriptTest::~LuaScriptTest()
{
}

bool LuaScriptTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	m_script = LuaScript::Create(
		"test.lua", 0 
			| LuaScript::Lib_LuaStandard 
			| LuaScript::Lib_FrmCore
			);

	if (!m_script)
	{
		return false;
	}

	m_script->execute(); 
	m_script->dbgPrintStack();

	auto hashCpp = StringHash("StringHash");
	auto hashLua = m_script->getValue<StringHash::HashType>("strHash");
	FRM_ASSERT(hashCpp == hashLua);

	return true;
}

void LuaScriptTest::shutdown()
{
	LuaScript::Destroy(m_script);

	AppBase::shutdown();
}

bool LuaScriptTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	static bool alwaysExecute = false;
	if (ImGui::Button("Execute") || alwaysExecute) {
		m_script->execute();
		m_script->dbgPrintStack();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Always Execute", &alwaysExecute);

	ImGui::Spacing(); ImGui::Spacing();
	static String<64> name = "globalVal";
	static int i = 0;
	static int v = 13;
	ImGui::InputText("name", (char*)name, name.getCapacity());
	ImGui::InputInt("i", &i);
	ImGui::InputInt("v", &v);
	ImGui::Spacing();
	if (ImGui::Button("next()"))
	{
		m_script->next();
		m_script->dbgPrintStack();
	}
	if (ImGui::Button("find(name)"))
	{
		m_script->find(name.c_str());
		m_script->dbgPrintStack();
	}
	if (ImGui::Button("getValue(i)"))
	{
		FRM_LOG("getValue(%d) = %d", i, m_script->getValue<int>(i));
		m_script->dbgPrintStack();
	}
	if (ImGui::Button("setValue(v, i)"))
	{
		m_script->setValue<int>(v, i);
		m_script->dbgPrintStack();
	}
	if (ImGui::Button("setValue(v, name)"))
	{
		m_script->setValue<int>(v, name.c_str());
		m_script->dbgPrintStack();
	}
	if (ImGui::Button("enterTable()"))
	{
		m_script->enterTable();
		m_script->dbgPrintStack();
	}
	ImGui::SameLine();
	if (ImGui::Button("leaveTable()"))
	{
		m_script->leaveTable();
		m_script->dbgPrintStack();
	}
	ImGui::Spacing();
	if (ImGui::Button("pushValue(v)")) 
	{
		m_script->pushValue<int>(v);
		m_script->dbgPrintStack();
	}
	if (ImGui::Button("call()"))
	{
		FRM_LOG("call() = %d", m_script->call());
		m_script->dbgPrintStack();
	}
	if (ImGui::Button("popValue()"))
	{
		FRM_LOG("popValue() = %d", m_script->popValue<int>());
		m_script->dbgPrintStack();
	}

	return true;
}

void LuaScriptTest::draw()
{
 // code here

	AppBase::draw();
}
