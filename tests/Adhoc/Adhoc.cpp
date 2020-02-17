#include "Adhoc.h"

#include <frm/core/frm.h>
#include <frm/core/log.h>
#include <frm/core/Mesh.h>
#include <frm/core/MeshData.h>

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

	Mesh::Create("test.gltf");
	return true;
}

void Adhoc::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool Adhoc::update()
{
	if (!AppBase::update())
	{
		return false;
	}

 // code here

	return true;
}

void Adhoc::draw()
{
 // code here

	AppBase::draw();
}
