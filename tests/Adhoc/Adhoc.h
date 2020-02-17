#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class Adhoc: public AppBase
{
public:
	Adhoc();
	virtual ~Adhoc();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
};
