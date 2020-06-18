#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class BasicRendererTest: public AppBase
{
public:
	BasicRendererTest();
	virtual ~BasicRendererTest();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	frm::BasicRenderer* m_basicRenderer = nullptr;
};
