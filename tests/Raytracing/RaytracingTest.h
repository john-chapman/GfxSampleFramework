#pragma once

#include <frm/core/AppSample3d.h>
#include <frm/core/RaytracingRenderer/RaytracingRenderer.h>

#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>

using namespace frm;

typedef AppSample3d AppBase;

class RaytracingTest: public AppBase
{
public:
	RaytracingTest();
	virtual ~RaytracingTest();

	virtual bool init(const ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	BasicRenderer*      m_basicRenderer      = nullptr;
	RaytracingRenderer* m_raytracingRenderer = nullptr;
	Texture*            m_txResult           = nullptr;
	ivec2               m_resolution         = ivec2(128);
	float*              m_result             = nullptr;
	bool                m_drawDebug          = true;
	int                 m_rayThreadCount     = 8;
	int                 m_raysPerThread      = 256;

	Shader*             m_shMaterialHit      = nullptr;
	Shader*             m_shMiss             = nullptr;
	struct HitResult
	{
		Buffer* bfHits;
		Buffer* bfResults;
		uint32  count;
		void*   instance;
	};
	using HitMap = eastl::unordered_map<void*, HitResult>;
	HitMap m_hitMap;

	void clearHitMap();
	void buildHitMap(const RaytracingRenderer::RayHit* _rayHits, size_t _count);

	void setResolution(const ivec2& _resolution);
};
