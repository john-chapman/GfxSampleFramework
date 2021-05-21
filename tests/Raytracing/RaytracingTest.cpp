#if FRM_MODULE_PHYSICS

#include <frm/core/frm.h>

#include <frm/core/Buffer.h>
#include <frm/core/Camera.h>
#include <frm/core/GlContext.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>
#include <frm/core/BasicRenderer/BasicRenderer.h>
#include <frm/core/RaytracingRenderer/RaytracingRenderer.h>
#include <frm/core/world/World.h>

#include <frm/physics/Physics.h>

#include "RaytracingTest.h"

#include <imgui/imgui.h>

static RaytracingTest s_inst;

RaytracingTest::RaytracingTest()
	: AppBase("Raytracing") 
{
}

RaytracingTest::~RaytracingTest()
{
}

bool RaytracingTest::init(const ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	m_basicRenderer = BasicRenderer::Create();
	m_raytracingRenderer = RaytracingRenderer::Create(m_rayThreadCount, m_raysPerThread);
	
	m_shMaterialHit = Shader::CreateCs("shaders/MaterialHit.glsl", 64);
	FRM_ASSERT(CheckResource(m_shMaterialHit));
	m_shMiss = Shader::CreateCs("shaders/Miss.glsl", 64);
	FRM_ASSERT(CheckResource(m_shMiss));

	setResolution(m_resolution);

	return true;
}

void RaytracingTest::shutdown()
{
	clearHitMap();

	Shader::Release(m_shMiss);
	Shader::Release(m_shMaterialHit);

	FRM_FREE(m_result);
	Texture::Release(m_txResult);
 
	RaytracingRenderer::Destroy(m_raytracingRenderer);
	BasicRenderer::Destroy(m_basicRenderer);
	
	AppBase::shutdown();
}

bool RaytracingTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	ImGui::InputInt2("Resolution", &m_resolution.x);
	if (ImGui::Button("Apply"))
	{
		setResolution(m_resolution);
	}

	bool reinitRaytracer = false;
	if (ImGui::InputInt("Ray thread count", &m_rayThreadCount))
	{
		m_rayThreadCount = Clamp(m_rayThreadCount, 1, 1024);
		reinitRaytracer = true;
	}

	if (ImGui::InputInt("Max rays/thread", &m_raysPerThread))
	{
		m_raysPerThread = Clamp(m_raysPerThread, 16, 8192);
		reinitRaytracer = true;
	}

	if (reinitRaytracer)
	{		
		RaytracingRenderer::Destroy(m_raytracingRenderer);
		m_raytracingRenderer = RaytracingRenderer::Create(m_rayThreadCount, m_raysPerThread);
	}

	ImGui::Checkbox("Draw Debug", &m_drawDebug);

	ImGui::SetNextWindowSize(ImVec2(512, 512), ImGuiCond_Once);
	if (ImGui::Begin("Output", nullptr, ImGuiWindowFlags_NoScrollbar))
	{
		ImVec2 viewSize = ImGui::GetContentRegionMax();
		ImGui::ImageButton((ImTextureID)m_txResult->getTextureView(), viewSize, ImVec2(0, 1), ImVec2(1, 0), 0);
		ImGui::End();
	}

	m_raytracingRenderer->update();
	if (m_drawDebug)
	{
		m_raytracingRenderer->drawDebug();
	}

	constexpr vec3 kFaceColors[] =
		{
			vec3(1.0f, 0.0f, 0.0f),
			vec3(0.0f, 1.0f, 0.0f),
			vec3(0.0f, 0.0f, 1.0f),
			vec3(1.0f, 0.0f, 1.0f),
			vec3(1.0f, 1.0f, 0.0f),
			vec3(0.0f, 1.0f, 1.0f)
		};
	
	#if 1
	{	PROFILER_MARKER_CPU("Raytrace");

		static eastl::vector<RaytracingRenderer::Ray>    rayList;
		static eastl::vector<RaytracingRenderer::RayHit> rayHitList;

		const size_t rayCount = m_resolution.x * m_resolution.y;
		rayList.resize(rayCount);
		rayHitList.resize(rayCount);

		Camera* camera = World::GetDrawCamera();
		for (int x = 0; x < m_resolution.x; ++x)
		{
			for (int y = 0; y < m_resolution.y; ++y)
			{
				const int i = (y * m_resolution.x + x);
				const vec2 ndc = vec2(x, y) / vec2(m_resolution) * 2.0f - 1.0f;
				const vec3 primaryRay = normalize(camera->getFrustumRayW(ndc));
				rayList[i] = { camera->getPosition(), primaryRay, 1e10f };
			}
		}

		auto MakeKey = [](const RaytracingRenderer::RayHit& _hit) -> uint64_t
		{
			if (!_hit.instance)
			{
				return 0;
			}
			uint64_t ret = (uint64_t)(_hit.instance);
			ret = BitfieldInsert(ret, (uint64_t)_hit.isHit, 63, 1);
			return ret;
		};

		m_raytracingRenderer->raycast(rayList.data(), rayHitList.data(), rayList.size());
		//m_raytracingRenderer->sortRayHits(rayHitList.data(), rayHitList.size()); // .22ms in release
		buildHitMap(rayHitList.data(), rayHitList.size()); // .41ms in release, mostly sending the data to GL.
		
		#if 1
		{
			PROFILER_MARKER("GPU hit results");

			GlContext* ctx = GlContext::GetCurrent();
			const int localSize = m_shMaterialHit->getLocalSize().x;
			for (auto& it : m_hitMap)
			{
				if (it.first == 0) // Key 0 = misses
				{
					continue;
				}

				if (it.second.count == 0)
				{
					continue;
				}

				ctx->setShader(m_shMaterialHit);
				ctx->bindBuffer("bfRayHits", it.second.bfHits);
				//ctx->bindBuffer("bfResult", it.second.bfResults);
				ctx->bindImage("txResult", m_txResult, GL_WRITE_ONLY);
				m_raytracingRenderer->bindInstance(ctx, it.second.instance);
				ctx->dispatch((it.second.count + localSize - 1) / localSize);
			}

			HitResult& misses = m_hitMap[0];
			if (misses.count > 0)
			{
				ctx->setShader(m_shMiss);
				ctx->bindBuffer("bfRayHits", misses.bfHits);
				ctx->bindImage("txResult", m_txResult, GL_WRITE_ONLY);
				ctx->dispatch((misses.count + localSize - 1) / localSize);			
			}

			glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
		}
		#else
		{
			const vec3 kColors[] = 
				{
					vec3(1, 0, 0),
					vec3(0, 1, 0),
					vec3(0, 0, 1),
					vec3(1, 0, 1),
					vec3(1, 1, 0),
					vec3(0, 1, 1)
				};
			memset(m_result, 0, sizeof(vec4) * m_resolution.x * m_resolution.y);
			for (size_t i = 0; i < rayHitList.size(); ++i)
			{
				const RaytracingRenderer::RayHit& rayHit = rayHitList[i];
				//if (!rayHit.isHit)
				//{
				//	break;
				//}
								
				vec3& result = *((vec3*)(m_result + rayHit.rayID * 4));
				if (rayHit.isHit)
				{		
					result = kColors[rayHit.triangleIndex % FRM_ARRAY_COUNT(kColors)];
					//result = vec3(rayHit.normal * 0.5f + 0.5f);	
					//result = vec3(rayHit.barycentrics, 0.0f);	

				}
				else
				{
					result = vec3(0.0f);
				}
			}
			m_txResult->setData(m_result, GL_RGBA, GL_FLOAT);
		}
		#endif
	}
	#endif

	return true;
}

void RaytracingTest::draw()
{
	Camera* drawCamera = World::GetDrawCamera();
	Camera* cullCamera = World::GetCullCamera();

	m_basicRenderer->nextFrame((float)getDeltaTime(), drawCamera, cullCamera);
	m_basicRenderer->draw((float)getDeltaTime(), drawCamera, cullCamera);

	AppBase::draw();
}

// PROTECTED

void RaytracingTest::clearHitMap()
{
	PROFILER_MARKER_CPU("RaytracingTest::clearHitMap");

	for (auto& it : m_hitMap)
	{
		HitResult& hitResult = it.second;
		Buffer::Destroy(hitResult.bfHits);
		Buffer::Destroy(hitResult.bfResults);
	}
	m_hitMap.clear();
}

void RaytracingTest::buildHitMap(const RaytracingRenderer::RayHit* _rayHits, size_t _count)
{
	PROFILER_MARKER_CPU("RaytracingTest::buildHitMap");

	clearHitMap();

	using RayHit = RaytracingRenderer::RayHit;

	eastl::unordered_map<void*, eastl::vector<RayHit> > tmpHitMap; // store sorted hits here.
	const RaytracingRenderer::RayHit* end = _rayHits + _count;
	for (; _rayHits != end; ++_rayHits)
	{
		// \todo Can avoid the overhead of calling reserve() by accessing a prexisting list of MeshData instances (or determine counts during hit generation).
		// ACTUALLY calling reserve is the main cost?
		eastl::vector<RayHit>& hitList = tmpHitMap[_rayHits->meshData];
		//hitList.reserve(_count + hitList.size());
		hitList.push_back(*_rayHits);
	}

	size_t totalCount = 0;
	for (auto& it : tmpHitMap)
	{
		HitResult& hitResult = m_hitMap[it.first];
		FRM_ASSERT(hitResult.bfHits == nullptr);
		hitResult.instance = it.second[0].instance; // \todo Needs more data e.g. matrix.
		hitResult.count = (uint32)it.second.size();
		hitResult.bfHits = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(RayHit) * hitResult.count, 0, it.second.data());
		hitResult.bfResults = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(vec3) * hitResult.count, 0, it.second.data());

		totalCount += hitResult.count;
	}

	FRM_ASSERT(totalCount == _count);
}

void RaytracingTest::setResolution(const ivec2& _resolution)
{
	Texture::Release(m_txResult);
	m_txResult = Texture::Create2d(_resolution.x, _resolution.y, GL_RGBA32F);
	m_txResult->setName("txResult");
	m_txResult->setFilter(GL_NEAREST);

	FRM_FREE(m_result);
	const int pixelCount = _resolution.x * _resolution.y;
	m_result = (float*)FRM_MALLOC(sizeof(vec4) * pixelCount);
}
#endif