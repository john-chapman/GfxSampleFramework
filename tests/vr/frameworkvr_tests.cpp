#include <frm/def.h>

#include <frm/AppSampleVr.h>
#include <frm/GlContext.h>
#include <frm/Input.h>
#include <frm/Window.h>

#include <apt/ArgList.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

class AppSampleTest: public AppSampleVr
{
public:
	typedef AppSampleVr AppBase;

	AppSampleTest(): AppBase("AppSampleVrTest") {}
	
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
		if (isVrMode()) {
			GlContext* ctx = getGlContext();
			for (int eye = 0; eye < Eye_Count; ++eye) {
				commitEye((Eye)eye, Layer_Main);
				commitEye((Eye)eye, Layer_Text);
			}
		}

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
		glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
		app->draw();
	}
	app->shutdown();

	return 0;
}
