#pragma once

#include <frm/core/AppSample.h>

typedef frm::AppSample AppBase;

class LuaScriptTest: public AppBase
{
public:
	LuaScriptTest();
	virtual ~LuaScriptTest();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	frm::LuaScript* m_script = nullptr;
};
