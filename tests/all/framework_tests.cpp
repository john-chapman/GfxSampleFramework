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
		Texture::Create("textures/test_512_rgba8.png");
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

		static vec3 position, scale;
		static quat orientation;
		Im3d::Gizmo("gizmoTest", &position, &orientation, &scale);

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
