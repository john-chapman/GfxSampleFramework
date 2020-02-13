#include "OctreeTest.h"

#include <frm/core/frm.h>

using namespace frm;

static OctreeTest s_inst;

OctreeTest::OctreeTest()
	: AppBase("Octree") 
{
}

OctreeTest::~OctreeTest()
{
}

bool OctreeTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

 // code here

	return true;
}

void OctreeTest::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool OctreeTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

 // code here

	return true;
}

void OctreeTest::draw()
{
 // code here

	AppBase::draw();
}
