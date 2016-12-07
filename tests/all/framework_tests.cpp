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
		

		Ray r;
		r.m_origin = m_scene.getCullCamera()->getPosition();
		r.m_direction = m_scene.getCullCamera()->getViewVector();
		//r = getCursorRayW();

	 // nearest point/distance tests

		Im3d::Color kLineColor = Im3d::Color(0.2f, 0.2f, 1.0f);

		Line ln(vec3(0.0f, 2.0f, 0.0f), normalize(vec3(1.0f, 1.0f, 0.0f)));
		LineSegment ls(vec3(-2.0f, 1.0f, 0.0f), vec3(2.0f, -1.0f, 1.0f));

		static vec3 testPos0(0.0f);
		static vec3 testPos1(-10.0f, 0.0f, 0.0f); 
		static vec3 testScale(1.0f);
		static quat testOri = quat(1.0f, 0.0f, 0.0f, 0.0f);
		Im3d::Gizmo("TestGizmo0", &testPos0, &testOri, &testScale);
		//Im3d::Gizmo("TestGizmo1", &testPos1, &testOri, &testScale);
	
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
