#include "QuadtreeTest.h"

#include <frm/core/frm.h>

using namespace frm;

static QuadtreeTest s_inst;

QuadtreeTest::QuadtreeTest()
	: AppBase("Quadtree") 
{
}

QuadtreeTest::~QuadtreeTest()
{
}

bool QuadtreeTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

 // code here

	return true;
}

void QuadtreeTest::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool QuadtreeTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

 // code here

	return true;
}

void QuadtreeTest::draw()
{
 // code here

	AppBase::draw();
}
