#include <frm/def.h>

#include <frm/AppSample3d.h>
#include <frm/AppProperty.h>
#include <frm/GlContext.h>
#include <frm/Input.h>
#include <frm/Mesh.h>
#include <frm/MeshData.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>
#include <frm/Texture.h>
#include <frm/Window.h>

#include <apt/ArgList.h>
#include <apt/IniFile.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

class AppSampleTest: public AppSample3d
{
public:
	typedef AppSample3d AppBase;

	Mesh* m_teapot;
	Shader* m_shModel;

	AppSampleTest(): AppBase("AppSampleTest") {}
	
	virtual bool init(const apt::ArgList& _args) override
	{
		if (!AppBase::init(_args)) {
			return false;
		}
		return true;
	}

	virtual void shutdown() override
	{
		AppBase::shutdown();
	}

	virtual bool update() override
	{
		if (!AppBase::update()) {
			return false;
		}

		static vec3 position0(-0.5f, 0.0f, 0.5f), scale0(1.0f);
		static quat orientation0(1.0f, 0.0f, 0.0f, 0.0f);
		Im3d::Gizmo("gizmoTest0", &position0, &orientation0, &scale0);

		static vec3 position1(-2.5f, 4.0f, 2.5f), scale1(1.0f);
		static quat orientation1(1.0f, 0.0f, 0.0f, 0.0f);
		Im3d::Gizmo("gizmoTest1", &position1, &orientation1, &scale1);


	 // primitive intersection tests
		Im3d::Color kLineColor     = Im3d::Color(0.5f, 1.0f, 0.5f);
		Im3d::Color kPlaneColor    = Im3d::Color(1.0f, 0.0f, 0.5f);
		Im3d::Color kBoxColor      = Im3d::Color(1.0f, 0.9f, 0.0f);
		Im3d::Color kSphereColor   = Im3d::Color(1.0f, 0.3f, 0.0f);
		Im3d::Color kCylinderColor = Im3d::Color(0.0f, 1.0f, 0.1f);
		Im3d::Color kCapsuleColor  = Im3d::Color(0.3f, 0.3f, 1.0f);

		Line ln(vec3(-2.0f, -2.0f, -2.0f), vec3(2.0f, 2.0f, 2.0f));
		Plane p(vec3(0.0f, 1.0f, 0.0f), 0.0f);
		AlignedBox ab(vec3(-8.0f, 0.0f, 4.0f), vec3(-4.0f, 2.0f, 5.0f));
		Sphere s(vec3(-1.0f, 1.0f, 4.5f), 1.5f);
		Cylinder cy(vec3(2.0f, 0.0f, 4.5f), vec3(2.0f, 3.0f, 4.5f), 1.0f);
		Capsule ca(vec3(4.0f, 1.5f, 4.5f), vec3(8.0f, 1.0f, 4.5f), 0.5f);

		Ray r;
		//r = getCursorRayW();
		r.m_origin = m_scene.getCullCamera()->getPosition();
		r.m_direction = m_scene.getCullCamera()->getViewVector();
		float tnear, tfar;

	 // line
		/*Im3d::SetColor(kLineColor);
		Im3d::BeginLines();
			Im3d::Vertex(ln.m_start, 2.0f);
			Im3d::Vertex(ln.m_end,   2.0f);
		Im3d::End();
		r.distance2(ln, tnear);
		Im3d::BeginPoints();
			Im3d::Vertex(r.m_origin + r.m_direction * tnear, 8.0f, kLineColor);
		Im3d::End();*/

	 // plane
		if (Intersects(r, p)) {
			Intersect(r, p, tnear);
			Im3d::BeginPoints();
				Im3d::Vertex(r.m_origin + r.m_direction * tnear, 8.0f, kPlaneColor);
			Im3d::End();
		}

	  // box
		Im3d::SetColor(kBoxColor);
		if (Intersect(r, ab, tnear, tfar)) {
			Im3d::BeginPoints();
				Im3d::Vertex(r.m_origin + r.m_direction * tfar,  8.0f);
				Im3d::Vertex(r.m_origin + r.m_direction * tnear, 8.0f);
			Im3d::End();
			Im3d::BeginLines();
				Im3d::Vertex(r.m_origin + r.m_direction * tfar,  1.0f);
				Im3d::Vertex(r.m_origin + r.m_direction * tnear, 1.0f);
			Im3d::End();
		}		
		kBoxColor.setA(frm::Intersects(r, ab) ? 1.0f : 0.4f);
		Im3d::SetColor(kBoxColor);
		Im3d::SetSize(2.0f);
		Im3d::DrawBox(ab.m_min, ab.m_max);

	 // sphere
		Im3d::SetColor(kSphereColor);
		if (Intersect(r, s, tnear, tfar)) {
			Im3d::BeginPoints();
				Im3d::Vertex(r.m_origin + r.m_direction * tfar,  8.0f);
				Im3d::Vertex(r.m_origin + r.m_direction * tnear, 8.0f);
			Im3d::End();
			Im3d::BeginLines();
				Im3d::Vertex(r.m_origin + r.m_direction * tfar,  1.0f);
				Im3d::Vertex(r.m_origin + r.m_direction * tnear, 1.0f);
			Im3d::End();
		}
		kSphereColor.setA(frm::Intersects(r, s) ? 1.0f : 0.4f);
		Im3d::SetColor(kSphereColor);
		Im3d::SetSize(2.0f);
		Im3d::DrawSphere(s.m_origin, s.m_radius, 32);

	 // cylinder
		Im3d::SetColor(kCylinderColor);
		//if (Intersect(r, cy, tnear, tfar)) {
		//	Im3d::BeginPoints();
		//		Im3d::Vertex(r.m_origin + r.m_direction * tfar,  8.0f);
		//		Im3d::Vertex(r.m_origin + r.m_direction * tnear, 8.0f);
		//	Im3d::End();
		//	Im3d::BeginLines();
		//		Im3d::Vertex(r.m_origin + r.m_direction * tfar,  1.0f);
		//		Im3d::Vertex(r.m_origin + r.m_direction * tnear, 1.0f);
		//	Im3d::End();
		//}
		//kCylinderColor.setA(frm::Intersects(r, cy) ? 1.0f : 0.4f);
		//Im3d::SetColor(kCylinderColor);
		//Im3d::SetSize(2.0f);
		//Im3d::DrawCylinder(cy.m_start, cy.m_end, cy.m_radius, 24);

	 // capsule
		Im3d::SetColor(kCapsuleColor);
		/*if (Intersect(r, ca, tnear, tfar)) {
			Im3d::BeginPoints();
				Im3d::Vertex(r.m_origin + r.m_direction * tfar,  8.0f);
				Im3d::Vertex(r.m_origin + r.m_direction * tnear, 8.0f);
			Im3d::End();
			Im3d::BeginLines();
				Im3d::Vertex(r.m_origin + r.m_direction * tfar,  1.0f);
				Im3d::Vertex(r.m_origin + r.m_direction * tnear, 1.0f);
			Im3d::End();
		}*/
		kCapsuleColor.setA(frm::Intersects(r, ca) ? 1.0f : 0.4f);
		Im3d::SetColor(kCapsuleColor);
		Im3d::SetSize(2.0f);
		Im3d::DrawCapsule(ca.m_start, ca.m_end, ca.m_radius, 24);

	 // ray
		Im3d::BeginLines();
			Im3d::Vertex(r.m_origin,  3.0f, Im3d::Color(0.0f, 1.0f, 0.0f));
			Im3d::Vertex(r.m_origin + r.m_direction, 3.0f, Im3d::Color(0.0f, 1.0f, 0.0f));
		Im3d::End();

		return true;
	}

	virtual void drawMainMenuBar() override
	{
		AppBase::drawMainMenuBar();
	}

	virtual void drawStatusBar() override
	{
		AppBase::drawStatusBar();
	}

	virtual void draw() override
	{
		GlContext* ctx = GlContext::GetCurrent();

		AppBase::draw();
	}
};
AppSampleTest s_app;

int main(int _argc, char** _argv)
{
	//TestJsonSerializer();

	AppSample* app = AppSample::GetCurrent();
	if (!app->init(ArgList(_argc, _argv))) {
		APT_ASSERT(false);
		return 1;
	}
	Window* win = app->getWindow();
	GlContext* ctx = app->getGlContext();
	while (app->update()) {
		APT_VERIFY(GlContext::MakeCurrent(ctx));
		glAssert(glViewport(0, 0, win->getWidth(), win->getHeight()));
		glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT));

		app->draw();
	}
	app->shutdown();

	return 0;
}
