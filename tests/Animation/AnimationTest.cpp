#include "AnimationTest.h"

#include <frm/core/frm.h>

using namespace frm;

static AnimationTest s_inst;

AnimationTest::AnimationTest()
	: AppBase("Animation") 
{
}

AnimationTest::~AnimationTest()
{
}

bool AnimationTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

 // code here

	return true;
}

void AnimationTest::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool AnimationTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

 // code here

	return true;
}

void AnimationTest::draw()
{
 // code here

	AppBase::draw();
}
