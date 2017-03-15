#include <frm/def.h>

#include <frm/interpolation.h>
#include <frm/gl.h>
#include <frm/AppSample3d.h>
#include <frm/AppProperty.h>
#include <frm/GlContext.h>
#include <frm/Input.h>
#include <frm/Mesh.h>
#include <frm/MeshData.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>
#include <frm/Spline.h>
#include <frm/Texture.h>
#include <frm/Window.h>
#include <frm/XForm.h>

#include <apt/ArgList.h>
#include <apt/IniFile.h>
#include <apt/Json.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

static void asin2f(float _theta, float& out0_, float& out1_)
{
	out0_ = asinf(_theta);
	out1_ = out0_ > 0.0f ? (out0_ + half_pi<float>()) : (out0_ - pi<float>());
}

class AppSampleTest: public AppSample3d
{
public:
	typedef AppSample3d AppBase;

	SplinePath m_splinePath;

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

		Ray r;
		r.m_origin = Scene::GetCullCamera()->getPosition();
		r.m_direction = Scene::GetCullCamera()->getViewVector();

		Cylinder cylinder(vec3(0.0f, 0.0f, -3.0f), vec3(0.0f, 0.0f, 3.0f), 1.5f);
		static mat4 cylinderMatrix(1.0f);
		Im3d::Gizmo("Cylinder", (float*)&cylinderMatrix);
		cylinder.transform(cylinderMatrix);

		Im3d::PushDrawState();
			Im3d::SetSize(1.0f);
			Im3d::SetColor(Im3d::Color_Red);
			Im3d::DrawPrism(cylinder.m_start, cylinder.m_end, cylinder.m_radius, 32);
			
			float t0, t1;
			bool intersects = Intersect(r, cylinder, t0, t1);
			if (!intersects) {
				ImGui::Text("No intersection");
			}			

			Im3d::BeginLines();
				Im3d::Vertex(r.m_origin + r.m_direction * t0, 2.0f, Im3d::Color_Blue);
				Im3d::Vertex(r.m_origin + r.m_direction * t1, 2.0f, Im3d::Color_Green);
			Im3d::End();
			Im3d::BeginPoints();
				Im3d::Vertex(r.m_origin + r.m_direction * t0, 8.0f, Im3d::Color_Blue);
				Im3d::Vertex(r.m_origin + r.m_direction * t1, 6.0f, Im3d::Color_Green);
			Im3d::End();

		Im3d::PopDrawState();
		


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
		glAssert(glClearColor(0.3f, 0.3f, 0.3f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

		app->draw();
	}
	app->shutdown();

	return 0;
}
