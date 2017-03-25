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
#include <imgui/imgui_internal.h>

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

	Mesh* m_mesh;
	Shader* m_shMesh;

	AppSampleTest(): AppBase("AppSampleTest") {}
	
	virtual bool init(const apt::ArgList& _args) override
	{
		if (!AppBase::init(_args)) {
			return false;

		}
		m_mesh = Mesh::Create("models/md5/bob_lamp_update.md5mesh");
		m_shMesh = Shader::CreateVsFs("shaders/Model_vs.glsl", "shaders/Model_fs.glsl");
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

		//ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Once);
		if (ImGui::TreeNode("Intersection Tests")) {
			Im3d::PushDrawState();

			enum Primitive
			{
				Primitive_Sphere,
				Primitive_Plane,
				Primitive_AlignedBox,
				Primitive_Cylinder,
				Primitive_Capsule
			};
			const char* primitiveList =
				"Sphere\0"
				"Plane\0"
				"AlignedBox\0"
				"Cylinder\0"
				"Capsule\0"
				;
			static int currentPrim = Primitive_Capsule;
			ImGui::Combo("Primitive", &currentPrim, primitiveList);
			static int useLine = 1;
			ImGui::RadioButton("Ray", &useLine, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Line", &useLine, 1);

			static mat4 primMat(1.0f);
			static float length = 3.0f;
			static float width  = 3.0f;
			static float radius = 1.0f;

			Ray ray;
			ray.m_origin = Scene::GetCullCamera()->getPosition();
			ray.m_direction = Scene::GetCullCamera()->getViewVector();
			Line line(ray.m_origin, ray.m_direction);
			bool intersects = false;
			bool intersectCheck = false;
			float t0, t1;

			#define Intersect1(prim) \
				if (useLine) { \
					intersects = Intersect(line, prim, t0); \
					intersectCheck = Intersects(line, prim); \
				} else { \
					intersects = Intersect(ray, prim, t0); \
					intersectCheck = Intersects(ray, prim); \
				}
			#define Intersect2(prim) \
				if (useLine) { \
					intersects = Intersect(line, prim, t0, t1); \
					intersectCheck = Intersects(line, prim); \
				} else { \
					intersects = Intersect(ray, prim, t0, t1); \
					intersectCheck = Intersects(ray, prim); \
				}
	
			Im3d::Gizmo("Primitive", (float*)&primMat);
			Im3d::SetColor(Im3d::Color_Red);
			Im3d::SetSize(3.0f);
			switch ((Primitive)currentPrim) {
				case Primitive_Sphere: {
					ImGui::SliderFloat("Radius", &radius, 0.0f, 8.0f);
					Sphere sphere(vec3(0.0f), radius);
					sphere.transform(primMat);
					Intersect2(sphere);
					Im3d::DrawSphere(sphere.m_origin, sphere.m_radius);
					break;
				}
				case Primitive_Plane: {
					ImGui::SliderFloat("Display Size", &width, 0.0f, 8.0f);
					Plane plane(vec3(0.0f, 1.0f, 0.0f), 0.0f);
					plane.transform(primMat);
					Intersect1(plane);
					t1 = t0;
					Im3d::DrawQuad(plane.getOrigin(), plane.m_normal, vec2(width));
					Im3d::BeginLines();
						Im3d::Vertex(plane.getOrigin());
						Im3d::Vertex(plane.getOrigin() + plane.m_normal);
					Im3d::End();
					break;
				}
				case Primitive_AlignedBox: {
					ImGui::SliderFloat("X", &length, 0.0f, 8.0f);
					ImGui::SliderFloat("Y",  &width,  0.0f, 8.0f);
					ImGui::SliderFloat("Z", &radius, 0.0f, 8.0f);
					AlignedBox alignedBox(vec3(-length, -width, -radius) * 0.5f, vec3(length, width, radius) * 0.5f);
					alignedBox.transform(primMat);
					Intersect2(alignedBox);
					Im3d::DrawAlignedBox(alignedBox.m_min, alignedBox.m_max);
					break;
				}
				case Primitive_Cylinder: {
					ImGui::SliderFloat("Length", &length, 0.0f, 8.0f);
					ImGui::SliderFloat("Radius", &radius, 0.0f, 8.0f);
					Cylinder cylinder(vec3(0.0f, -length * 0.5f, 0.0f), vec3(0.0f, length * 0.5f, 0.0f), radius);
					cylinder.transform(primMat);
					Intersect2(cylinder);
					Im3d::DrawCylinder(cylinder.m_start, cylinder.m_end, cylinder.m_radius);
					break;
				}
				case Primitive_Capsule:	{
					ImGui::SliderFloat("Length", &length, 0.0f, 8.0f);
					ImGui::SliderFloat("Radius", &radius, 0.0f, 8.0f);
					Capsule capsule(vec3(0.0f, -length * 0.5f, 0.0f), vec3(0.0f, length * 0.5f, 0.0f), radius);
					capsule.transform(primMat);
					Intersect2(capsule);
					Im3d::DrawCapsule(capsule.m_start, capsule.m_end, capsule.m_radius);
					break;
				}
				default:
					APT_ASSERT(false);
					break;
			};
			#undef Intersect1
			#undef Intersect2

			ImGui::Text("Intersects: %s", intersects ? "TRUE" : "FALSE");
			ImGui::SameLine();
			ImGui::TextColored((intersectCheck == intersects) ? ImColor(0.0f, 1.0f, 0.0f) : ImColor(1.0f, 0.0f, 0.0f), "+");
			Im3d::PushAlpha(0.7f);
			Im3d::BeginLines();
				if (useLine) {
					Im3d::Vertex(line.m_origin - line.m_direction * 999.0f, 1.0f, Im3d::Color_Cyan);
					Im3d::Vertex(line.m_origin + line.m_direction * 999.0f, 1.0f, Im3d::Color_Cyan);
				} else {
					Im3d::Vertex(ray.m_origin, 1.0f, Im3d::Color_Cyan);
					Im3d::Vertex(ray.m_origin + ray.m_direction * 999.0f, 1.0f, Im3d::Color_Cyan);
				}
			Im3d::End();
			Im3d::PopAlpha();
			if (intersects) {
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
 
			Im3d::PopDrawState();

			static bool enablePerf = false;	
			ImGui::Checkbox("Perf Test", &enablePerf);
			if (enablePerf) {
				static int opCount = 100000;
				ImGui::SliderInt("Op Count", &opCount, 1, 10000);
				double avg;
				#define PerfTest1(prim) \
					Timestamp t = Time::GetTimestamp(); \
					for (int i = 0; i < opCount; ++i) \
						Intersect(ray, prim, t0); \
					avg = (Time::GetTimestamp() - t).asMicroseconds();
				#define PerfTest2(prim) \
					Timestamp t = Time::GetTimestamp(); \
					for (int i = 0; i < opCount; ++i) \
						Intersect(ray, prim, t0, t1); \
					avg = (Time::GetTimestamp() - t).asMicroseconds();

				switch ((Primitive)currentPrim) {
					case Primitive_Sphere: {
						Sphere sphere(vec3(0.0f), radius);
						sphere.transform(primMat);
						PerfTest2(sphere);
						break;
					}
					case Primitive_Plane: {
						Plane plane(vec3(0.0f, 1.0f, 0.0f), 0.0f);
						plane.transform(primMat);
						PerfTest1(plane);
						break;
					}
					case Primitive_AlignedBox: {
						AlignedBox alignedBox(vec3(-length, -width, -radius) * 0.5f, vec3(length, width, radius) * 0.5f);
						alignedBox.transform(primMat);
						PerfTest2(alignedBox);
						break;
					}
					case Primitive_Cylinder: {
						Cylinder cylinder(vec3(0.0f, -length * 0.5f, 0.0f), vec3(0.0f, length * 0.5f, 0.0f), radius);
						cylinder.transform(primMat);
						PerfTest2(cylinder);
						break;
					}
					case Primitive_Capsule: {
						Capsule capsule(vec3(0.0f, -length * 0.5f, 0.0f), vec3(0.0f, length * 0.5f, 0.0f), radius);
						capsule.transform(primMat);
						PerfTest2(capsule);
						break;
					}
					default:
						APT_ASSERT(false);
						break;
				};
				#undef PerfTest1
				#undef PerfTest2
				avg /= (double)opCount;
				ImGui::Text("%fus", (float)avg);
			}

			ImGui::TreePop();
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
		GlContext* ctx = GlContext::GetCurrent();

		static mat4 meshWorld(1.0f);
		Im3d::Gizmo("MeshWorld", (float*)&meshWorld);

		ctx->setFramebufferAndViewport(0);
		glAssert(glClear(GL_DEPTH_BUFFER_BIT));

		static int submesh = 0;
		ImGui::SliderInt("Submesh", &submesh, 0, m_mesh->getSubmeshCount() - 1);
		ctx->setMesh(m_mesh, submesh);
		ctx->setShader(m_shMesh);

		ctx->setUniform("uWorldMatrix", meshWorld);
		ctx->setUniform("uViewMatrix", Scene::GetDrawCamera()->m_view);
		ctx->setUniform("uProjMatrix", Scene::GetDrawCamera()->m_proj);

		glAssert(glEnable(GL_DEPTH_TEST));
		glAssert(glEnable(GL_CULL_FACE));
		ctx->draw();
		glAssert(glDisable(GL_CULL_FACE));
		glAssert(glDisable(GL_DEPTH_TEST));

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
