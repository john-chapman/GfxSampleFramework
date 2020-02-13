#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class _skeleton: public AppBase
{
public:
	_skeleton();
	virtual ~_skeleton();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
};
