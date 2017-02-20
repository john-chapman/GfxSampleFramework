#include <frm/def.h>

#include <frm/interpolation.h>
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

		/*LCG lcg(1223781730);
		for (int i = 0; i < 4; ++i) {
			m_splinePath.append(vec3(
				lcg.frand(-10.0f, 10.0f),
				lcg.frand(2.0f,  5.0f),
				lcg.frand(-10.0f, 10.0f)
				));
		}
		m_splinePath.build();

		Scene& scene = Scene::GetCurrent();
		Camera* splineCam = scene.createCamera(Camera());
		Node* splineNode = splineCam->getNode();
		splineNode->setName("#SplineNode");
		splineNode->setActive(true);
		splineNode->setDynamic(true);
		XForm_SplinePath* splineXForm = (XForm_SplinePath*)XForm::Create(StringHash("XForm_SplinePath"));
		splineXForm->m_path = &m_splinePath;
		splineXForm->m_duration = 50.0f;
		splineNode->addXForm(splineXForm);
		XForm_LookAt* lookAtXForm = (XForm_LookAt*)XForm::Create(StringHash("XForm_LookAt"));
		lookAtXForm->m_target = scene.getRoot();
		splineNode->addXForm(lookAtXForm);
		*/
		
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
		
		static int s_sampleCount = 200;
        static int s_funcType = 0;
		static float s_min = 0.0f;
		static float s_max = 1.0f;


	 // interpolationt vis
	/*	struct Funcs {
			static float GetDelta(int _i)        { return s_min + ((float)_i / (float)s_sampleCount) * (s_max - s_min); }
            static float Lerp(void*, int _i)     { return lerp(0.0f, 1.0f, GetDelta(_i)); }
            static float Coserp(void*, int _i)   { return coserp(0.0f, 1.0f, GetDelta(_i)); }
			static float Cuberp(void*, int _i)   { return cuberp(0.0f, 0.25f, 0.75f, 1.0f, GetDelta(_i)); }
			static float Smooth(void*, int _i)   { return smooth(0.0f, 1.0f, GetDelta(_i)); }
			static float Accelerp(void*, int _i) { return accelerp(0.0f, 1.0f, GetDelta(_i)); }
			static float Decelerp(void*, int _i) { return decelerp(0.0f, 1.0f, GetDelta(_i)); }
        };
		ImGui::SliderFloat("Min", &s_min, -0.1f, 1.1f);
		ImGui::SliderFloat("Max", &s_max, -0.1f, 1.1f);
		ImGui::PushItemWidth(100.0f);
			ImGui::Combo("##Interp", &s_funcType, "lerp\0coserp\0cuberp\0smooth\0accelerp\0decelerp\0");;
		ImGui::PopItemWidth();
        float (*func)(void*, int) = NULL;
		float (*pfInterp)(float, float, float);
		vec3 (*pfInterp3d)(const vec3&, const vec3&, float);
		switch (s_funcType) {
			case 5:  func = Funcs::Decelerp; pfInterp = decelerp; pfInterp3d = decelerp; break;
			case 4:  func = Funcs::Accelerp; pfInterp = accelerp; pfInterp3d = accelerp; break;
			case 3:  func = Funcs::Smooth;   pfInterp = smooth;   pfInterp3d = smooth;   break;
			case 2:  func = Funcs::Cuberp;   pfInterp = lerp;     pfInterp3d = lerp;     break;
			case 1:  func = Funcs::Coserp;   pfInterp = coserp;   pfInterp3d = coserp;   break;
			case 0:
			default: func = Funcs::Lerp;     pfInterp = lerp;     pfInterp3d = lerp;     break;
		};
		ImGui::SameLine();
		ImGui::PlotLines("##Lines", func, NULL, 200, 0, NULL, 0.0f, 1.0f, ImVec2(0,80));
		m_splinePath.edit();
	*/	
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
		glAssert(glClearColor(0.3f, 0.3f, 0.3f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT));

		app->draw();
	}
	app->shutdown();

	return 0;
}
