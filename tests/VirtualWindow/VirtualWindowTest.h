#pragma once

#include <frm/core/AppSample.h>

typedef frm::AppSample AppBase;

class VirtualWindowTest: public AppBase
{
public:
	VirtualWindowTest();
	virtual ~VirtualWindowTest();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	bool       m_scrollBars = true;
	frm::ivec2 m_sizeW      = frm::ivec2(512);
	frm::vec2  m_sizeV      = frm::vec2(5);

};
