#include <frm/core/frm.h>

#include "_skeleton.h"

static _skeleton s_inst;

_skeleton::_skeleton()
	: AppBase("_skeleton") 
{
}

_skeleton::~_skeleton()
{
}

bool _skeleton::init(const ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

 // code here

	return true;
}

void _skeleton::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool _skeleton::update()
{
	if (!AppBase::update())
	{
		return false;
	}

 // code here

	return true;
}

void _skeleton::draw()
{
 // code here

	AppBase::draw();
}
