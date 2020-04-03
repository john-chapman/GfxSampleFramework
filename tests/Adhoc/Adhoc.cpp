#include "Adhoc.h"

#include <frm/core/frm.h>
#include <frm/core/log.h>

using namespace frm;

static Adhoc s_inst;

Adhoc::Adhoc()
	: AppBase("Adhoc") 
{
}

Adhoc::~Adhoc()
{
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

	return true;
}

void Adhoc::draw()
{
 // code here

	AppBase::draw();
}
