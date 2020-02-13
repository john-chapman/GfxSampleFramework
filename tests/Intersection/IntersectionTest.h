#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class IntersectionTest: public AppBase
{
public:
	IntersectionTest();
	virtual ~IntersectionTest();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
	enum Primitive_
	{
		Primitive_Sphere,
		Primitive_Plane,
		Primitive_AlignedBox,
		Primitive_Cylinder,
		Primitive_Capsule,

		Primitive_Count
	};
	typedef int Primitive;

	Primitive m_primitive            = Primitive_Sphere;
	frm::mat4 m_primitiveTransform   = frm::identity;
	float     m_primitiveLength      = 3.0f;
	float     m_primitiveWidth       = 3.0f;
	float     m_primitiveRadius      = 1.0f;
	bool      m_useLine              = false;

};
