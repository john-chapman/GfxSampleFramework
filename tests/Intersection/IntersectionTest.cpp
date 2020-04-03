#include "IntersectionTest.h"

#include <frm/core/frm.h>
#include <frm/core/geom.h>
#include <frm/core/Camera.h>
#include <frm/core/Properties.h>
#include <frm/core/Scene.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;

static IntersectionTest s_inst;

IntersectionTest::IntersectionTest()
	: AppBase("Intersection") 
{
	Properties::PushGroup("Intersection");
		//              name                     default                  min     max                   storage
		Properties::Add("m_useLine",             m_useLine,                                             &m_useLine);
		Properties::Add("m_primitive",           m_primitive,             0,      (int)Primitive_Count, &m_primitive);
		Properties::Add("m_primitiveLength",     m_primitiveLength,       1e-2f,  1e2f,                 &m_primitiveLength);
		Properties::Add("m_primitiveWidth",       m_primitiveWidth,       1e-2f,  1e2f,                 &m_primitiveWidth);
		Properties::Add("m_primitiveRadius",     m_primitiveRadius,       1e-2f,  1e2f,                 &m_primitiveRadius);
	Properties::PopGroup();
}

IntersectionTest::~IntersectionTest()
{
}

bool IntersectionTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

 // code here

	return true;
}

void IntersectionTest::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool IntersectionTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	Camera* cullCamera = Scene::GetCullCamera();

	Im3d::PushDrawState();

	ImGui::Combo("Primitive", &m_primitive,
		"Sphere\0"
		"Plane\0"
		"AlignedBox\0"
		"Cylinder\0"
		"Capsule\0"
		);
	if (ImGui::TreeNode("Primitive Size"))
	{
		bool editLength = false;
		bool editWidth  = false;
		bool editRadius = false;

		switch (m_primitive)
		{
			default:
			case Primitive_Sphere:      editRadius = true; break;
			case Primitive_Plane:       editWidth  = true; break;
			case Primitive_AlignedBox:  editLength = editWidth  = editRadius = true; break;
			case Primitive_Cylinder:    editLength = editRadius = true; break;
			case Primitive_Capsule:     editLength = editRadius = true; break;
		};

		if (editLength)
		{
			ImGui::SliderFloat("Length", &m_primitiveLength, 1e-2f, 8.0f);
		}

		if (editWidth)
		{
			ImGui::SliderFloat("Width", &m_primitiveWidth, 1e-2f, 8.0f);
		}

		if (editRadius)
		{
			ImGui::SliderFloat((m_primitive == Primitive_AlignedBox) ? "Height" : "Radius", &m_primitiveRadius, 1e-2f, 8.0f);
		}

		Im3d::Gizmo("primitiveTransform", (float*)&m_primitiveTransform);

		ImGui::TreePop();
	}

	int useLine = m_useLine ? 1 : 0;
	ImGui::RadioButton("Ray", &useLine, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Line", &useLine, 1);
	m_useLine = !!useLine;

	Ray ray;
	ray.m_origin = cullCamera->getPosition();//- vec3(0.0f, 1.0f, 0.0f);
	ray.m_direction = cullCamera->getViewVector();
	Line line(ray.m_origin, ray.m_direction);
	bool rayIntersects = false;
	bool lineIntersects = false;
	float t0, t1;
	t0 = t1 = 0.0f;

	Im3d::SetSize(3.0f);
	Im3d::SetColor(Im3d::Color_Red);
	const mat4 transform = TransformationMatrix(GetTranslation(m_primitiveTransform), GetRotation(m_primitiveTransform));

	double time = 0.0;
	const int opCount = 1000;
	#define PerfTest1(prim) \
		Timestamp t = Time::GetTimestamp(); \
		for (int i = 0; i < opCount; ++i) \
			Intersect(ray, prim, t0); \
		time = (Time::GetTimestamp() - t).asMicroseconds() / (double)opCount;
	#define PerfTest2(prim) \
		Timestamp t = Time::GetTimestamp(); \
		for (int i = 0; i < opCount; ++i) \
			Intersect(ray, prim, t0, t1); \
		time = (Time::GetTimestamp() - t).asMicroseconds() / (double)opCount;

	switch (m_primitive)
	{
		default:
		case Primitive_Sphere:
		{
			Sphere sphere(vec3(0.0f), m_primitiveRadius);
			sphere.transform(transform);
			rayIntersects = Intersect(ray, sphere, t0, t1);
			lineIntersects = Intersect(line, sphere, t0, t1);
			PerfTest2(sphere);

			Im3d::SetAlpha(1.0f);
			Im3d::DrawSphere(sphere.m_origin, sphere.m_radius, 64);
			Im3d::SetAlpha(0.1f);
			Im3d::DrawSphereFilled(sphere.m_origin, sphere.m_radius, 64);
			break;
		};
		case Primitive_Plane:
		{
			Plane plane(vec3(0.0f, 1.0f, 0.0f), 0.0f);
			plane.transform(transform);
			rayIntersects = Intersect(ray, plane, t0);
			lineIntersects = Intersect(line, plane, t0);
			t1 = t0;
			PerfTest1(plane);

			Im3d::SetAlpha(1.0f);
			Im3d::DrawQuad(plane.getOrigin(), plane.m_normal, vec2(m_primitiveWidth));
			Im3d::DrawLine(plane.getOrigin(), plane.getOrigin() + plane.m_normal * m_primitiveWidth * 0.5f, Im3d::GetSize(), Im3d::GetColor());
			Im3d::SetAlpha(0.1f);
			Im3d::DrawQuadFilled(plane.getOrigin(), plane.m_normal, vec2(m_primitiveWidth));
			break;
		};
		case Primitive_AlignedBox:
		{
			vec3 size = vec3(m_primitiveWidth, m_primitiveRadius, m_primitiveLength) * 0.5f;
			AlignedBox box(-size, size);
			box.transform(transform);
			rayIntersects = Intersect(ray, box, t0, t1);
			lineIntersects = Intersect(line, box, t0, t1);
			PerfTest2(box);

			Im3d::SetAlpha(1.0f);
			Im3d::DrawAlignedBox(box.m_min, box.m_max);
			Im3d::SetAlpha(0.1f);
			Im3d::DrawAlignedBoxFilled(box.m_min, box.m_max);			
			break;
		};
		case Primitive_Cylinder:
		{
			Cylinder cylinder(vec3(0.0f, -m_primitiveLength * 0.5f, 0.0f), vec3(0.0f, m_primitiveLength * 0.5f, 0.0f), m_primitiveRadius);
			cylinder.transform(transform);
			rayIntersects = Intersect(ray, cylinder, t0, t1);
			lineIntersects = Intersect(line, cylinder, t0, t1);
			PerfTest2(cylinder);
			
			Im3d::SetAlpha(1.0f);
			Im3d::DrawCylinder(cylinder.m_start, cylinder.m_end, cylinder.m_radius, 32);
			Im3d::SetAlpha(0.1f);
			//Im3d::DrawCylinderFilled(cylinder.m_start, cylinder.m_end, cylinder.m_radius, 16);
			break;
		};
		case Primitive_Capsule:
		{
			Capsule capsule(vec3(0.0f, -m_primitiveLength * 0.5f, 0.0f), vec3(0.0f, m_primitiveLength * 0.5f, 0.0f), m_primitiveRadius);
			capsule.transform(transform);
			rayIntersects = Intersect(ray, capsule, t0, t1);
			lineIntersects = Intersect(line, capsule, t0, t1);
			PerfTest2(capsule);
			
			Im3d::SetAlpha(1.0f);
			Im3d::DrawCapsule(capsule.m_start, capsule.m_end, capsule.m_radius, 32);
			Im3d::SetAlpha(0.1f);
			//Im3d::DrawCylinderFilled(cylinder.m_start, cylinder.m_end, cylinder.m_radius, 16);

			break;
		};
	};

	bool intersects = m_useLine ? lineIntersects : rayIntersects;

	Im3d::SetAlpha(0.75f);
	Im3d::BeginLines();
	if (useLine)
	{
		Im3d::Vertex(line.m_origin - line.m_direction * 999.0f, 2.0f, Im3d::Color_Cyan);
		Im3d::Vertex(line.m_origin + line.m_direction * 999.0f, 2.0f, Im3d::Color_Cyan);
	}
	else
	{
		Im3d::Vertex(ray.m_origin, 2.0f, Im3d::Color_Cyan);
		Im3d::Vertex(ray.m_origin + ray.m_direction * 999.0f, 2.0f, Im3d::Color_Cyan);
	}
	Im3d::End();

	Im3d::SetAlpha(1.0f);
	ImGui::Text("Intersects: %s", intersects ? "TRUE" : "FALSE");
	if (intersects)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImColor(0.0f, 0.0f, 1.0f), "t0 %.3f", t0);
		ImGui::SameLine();
		ImGui::TextColored(ImColor(0.0f, 1.0f, 0.0f), "t1 %.3f", t1);
		Im3d::BeginLines();
			Im3d::Vertex(ray.m_origin + ray.m_direction * t0, Im3d::Color_Blue);
			Im3d::Vertex(ray.m_origin + ray.m_direction * t1, Im3d::Color_Green);
		Im3d::End();
		Im3d::BeginPoints();
			Im3d::Vertex(ray.m_origin + ray.m_direction * t0, 8.0f, Im3d::Color_Blue);
			Im3d::Vertex(ray.m_origin + ray.m_direction * t1, 6.0f, Im3d::Color_Green);
		Im3d::End();			
	}
	ImGui::Text("%fus", time);


	Im3d::PopDrawState();

	return true;
}

void IntersectionTest::draw()
{

	AppBase::draw();
}
