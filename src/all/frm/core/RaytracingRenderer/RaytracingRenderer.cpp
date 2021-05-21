#if FRM_MODULE_PHYSICS

#include "RaytracingRenderer.h"

#include <frm/core/hash.h>
#include <frm/core/types.h>
#include <frm/core/Buffer.h>
#include <frm/core/DrawMesh.h>
#include <frm/core/GlContext.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Log.h>
#include <frm/core/Mesh.h>
#include <frm/core/Pool.h>
#include <frm/core/Profiler.h>
#include <frm/core/BasicRenderer/BasicLightComponent.h>
#include <frm/core/BasicRenderer/BasicMaterial.h>
#include <frm/core/BasicRenderer/BasicRenderableComponent.h>
#include <frm/core/BasicRenderer/ImageLightComponent.h>
#include <frm/core/world/World.h>

#include <frm/physics/PhysicsInternal.h>

#include <im3d/im3d.h>
#include <imgui/imgui.h>

#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <EASTL/sort.h>

#include <atomic>
#include <thread>

#pragma warning(disable: 4267) // Conversion size_t -> uint32_t.

namespace frm {

struct RaytracingRenderer::MeshData
{
	physx::PxGeometryHolder   pxGeometry;

	uint64_t                  key          = 0;
	int                       refCount     = 0;
	BasicMaterial*            material     = nullptr;
	Buffer*                   bfVertexData = nullptr;
	Buffer*                   bfIndexData  = nullptr;

	static uint64_t MakeKey(Mesh* _mesh, BasicMaterial* _material)
	{
		uint64_t ret = 0;
		ret = HashString<uint64_t>(_mesh->getPath(), ret);
		ret = Hash<uint64_t>(&_material, sizeof(BasicMaterial*), ret);
		return ret;
	}
};

struct RaytracingRenderer::Instance
{
	physx::PxRigidActor*      pxRigidActor = nullptr;
	physx::PxShape*           pxShape      = nullptr;

	SceneNode*                sceneNode    = nullptr;
	MeshData*                 meshData     = nullptr;
};

struct RaytracingRenderer::Impl
{
	using InstanceList = eastl::vector<RaytracingRenderer::Instance*>;
	using SceneMap     = eastl::unordered_map<BasicRenderableComponent*, InstanceList>;
	using MeshDataMap  = eastl::unordered_map<uint64_t, MeshData*>;
	using ThreadPool   = eastl::vector<std::thread>;
	
	physx::PxMaterial*        pxMaterial = nullptr;
	physx::PxScene*           pxScene    = nullptr;

	SceneMap                  sceneMap;
	Pool<Instance>            instancePool;
	MeshDataMap               meshDataMap;
	Pool<MeshData>            meshDataPool;

	struct RayJobList
	{
		const Ray*           raysIn  = nullptr;
		RayHit*              raysOut = nullptr;
		std::atomic<uint32>  readAt;
		std::atomic<uint32>  count;
	};

	ThreadPool                threadPool;
	eastl::vector<float>      jobsPerThread;
	bool                      threadShutdown = false;
	RayJobList                rayJobs;
	uint32                    maxJobsPerThrad = 512;

	Impl(): instancePool(256), meshDataPool(256) {}

	uint32 processJobs(uint32 _max)
	{		
		uint32 i, n, count;
		while (true)
		{
			count = rayJobs.count.load();
			i = rayJobs.readAt.load();
			n = Min(i + _max, count);
			
			if (rayJobs.readAt.compare_exchange_weak(i, n))
			{
				break;
			}
		}

		pxScene->lockRead();

		const physx::PxHitFlags flags = (physx::PxHitFlags)0
			| physx::PxHitFlag::ePOSITION
			| physx::PxHitFlag::eNORMAL
			| physx::PxHitFlag::eUV
			| physx::PxHitFlag::eFACE_INDEX
			//| physx::PxHitFlag::eMESH_ANY
			;

		uint32 ret = n - i;
		for (; i < n; ++i)
		{
			const Ray& rayIn = rayJobs.raysIn[i];
			RayHit& rayHit = rayJobs.raysOut[i];
			rayHit = RayHit();
			rayHit.isHit = 0;
			rayHit.rayID = i;

			physx::PxRaycastBuffer queryResult;
			if (!pxScene->raycast(Vec3ToPx(rayIn.origin), Vec3ToPx(rayIn.direction), rayIn.maxDistance, queryResult) || !queryResult.hasBlock)
			{
				continue;
			}
			Instance* instance   = (Instance*)queryResult.block.actor->userData;
			MeshData* meshData   = instance->meshData;

			rayHit.isHit         = 1;
			rayHit.position      = PxToVec3(queryResult.block.position);
			rayHit.normal        = PxToVec3(queryResult.block.normal);
			rayHit.distance      = queryResult.block.distance;
			rayHit.barycentrics  = vec2(queryResult.block.u, queryResult.block.v);
			rayHit.instance      = instance;			
			rayHit.triangleIndex = meshData->pxGeometry.triangleMesh().triangleMesh->getTrianglesRemap()[queryResult.block.faceIndex];
			rayHit.meshData      = meshData;
		}

		pxScene->unlockRead();

		// \todo Accessing threadPool is invalid here, it can happen while we're setting up the threadPool :*(
		bool found = false;
		for (std::thread& t : threadPool)
		{
			if (t.get_id() == std::this_thread::get_id())
			{
				jobsPerThread[&t - threadPool.begin() + 1] += (float)ret;
				found = true;
				break;
			}
		}
		if (!found)
		{
			jobsPerThread[0] += (float)ret;
		}

		return ret;
	}

	static void ThreadFunc(RaytracingRenderer::Impl* _impl)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // \hack \todo processJobs() accesses the threadPool, can sometimes crash during startup
		while (!_impl->threadShutdown)
		{
			_impl->processJobs(_impl->maxJobsPerThrad);
			std::this_thread::yield();
		}
	}
};


// PUBLIC

RaytracingRenderer* RaytracingRenderer::Create(uint32 _threadCount, uint32 _maxRaysPerThread)
{
	RaytracingRenderer* ret = FRM_NEW(RaytracingRenderer);
	ret->init(_threadCount, _maxRaysPerThread);
	return ret;
}

void RaytracingRenderer::Destroy(RaytracingRenderer*& _inst_)
{
	FRM_DELETE(_inst_);
	_inst_ = nullptr;
}

void RaytracingRenderer::update()
{
	PROFILER_MARKER_CPU("RaytracingRenderer::udpate");

	// \todo Simulate/fetch results don't need to be called as the scene is static.
	//m_impl->pxScene->simulate(1.0f/60.0f);
	//FRM_VERIFY(m_impl->pxScene->fetchResults(true)); // true = block until the results are ready

	// For each new renderable, create a transient physics component from its draw mesh.
	// The component will be destroyed with the parent node, we remove it from our own scene map via the OnNodeShutdown callback.
	auto& activeRenderables = BasicRenderableComponent::GetActiveComponents();
	for (BasicRenderableComponent* renderable : activeRenderables)
	{
		if (m_impl->sceneMap.find(renderable) == m_impl->sceneMap.end())
		{
			const SceneNode* node = renderable->getParentNode();
			if (!(node->isActive() && node->isStatic()))
			{
				continue; // Discard inactive and non-static nodes.
			}

			addInstances(renderable);
		}
	}

	// Loop over all instances, update world transforms.
	for (auto& it : m_impl->sceneMap)
	{
		for (Instance* instance : it.second)
		{
			// Calling setGlobalPose() on static actors may incur a performance penalty, so only do it if the pose changed.
			const physx::PxTransform world = Mat4ToPxTransform(instance->sceneNode->getWorld());
			if (!(world == instance->pxRigidActor->getGlobalPose()))
			{
				instance->pxRigidActor->setGlobalPose(world);
			}
		}
	}
}

void RaytracingRenderer::drawDebug()
{
	PROFILER_MARKER_CPU("RaytracingRenderer::drawDebug");

	if (ImGui::Begin("RaytracingRenderer"))
	{
		ImGui::Text("# instances: %u", m_impl->instancePool.getUsedCount());
		ImGui::Text("# mesh data: %u", m_impl->meshDataPool.getUsedCount());
		ImGui::PlotHistogram("Jobs/thread", m_impl->jobsPerThread.data(), (int)m_impl->jobsPerThread.size(), 0, nullptr, 0, 2048, ImVec2(0, 100));

		ImGui::End();
	}
return;

	auto PxToIm3dColor = [](physx::PxU32 c) -> Im3d::Color
		{
			return Im3d::Color(0
				| ((c & 0x00ff0000) << 8)
				| ((c & 0x0000ff00) << 8)
				| ((c & 0x000000ff) << 8)
				| ((c & 0xff000000) >> 24)
				);
		};
	
	// \todo This seems to have a 1 frame latency, calling DrawDebug() before/after the update doesn't seem to have any effect.
	const physx::PxRenderBuffer& drawList = m_impl->pxScene->getRenderBuffer();

	Im3d::PushDrawState();

	Im3d::BeginTriangles();
		for (auto i = 0u; i < drawList.getNbTriangles(); ++i)
		{
			const physx::PxDebugTriangle& tri = drawList.getTriangles()[i];
			Im3d::Vertex(PxToVec3(tri.pos0), PxToIm3dColor(tri.color0));
			Im3d::Vertex(PxToVec3(tri.pos1), PxToIm3dColor(tri.color1));
			Im3d::Vertex(PxToVec3(tri.pos2), PxToIm3dColor(tri.color2));				
		}
	Im3d::End();

	Im3d::SetSize(2.0f); // \todo parameterize
	Im3d::BeginLines();
		for (auto i = 0u; i < drawList.getNbLines(); ++i)
		{
			const physx::PxDebugLine& line = drawList.getLines()[i];
			Im3d::Vertex(PxToVec3(line.pos0), PxToIm3dColor(line.color0));
			Im3d::Vertex(PxToVec3(line.pos1), PxToIm3dColor(line.color1));
		}
	Im3d::End();

	Im3d::BeginPoints();
		for (auto i = 0u; i < drawList.getNbPoints(); ++i)
		{
			const physx::PxDebugPoint& point = drawList.getPoints()[i];
			Im3d::Vertex(PxToVec3(point.pos), PxToIm3dColor(point.color));
		}
	Im3d::End();

	for (physx::PxU32 i = 0; i < drawList.getNbTexts(); ++i)
	{
		const physx::PxDebugText& text = drawList.getTexts()[i];
		Im3d::Text(PxToVec3(text.position), text.size, PxToIm3dColor(text.color), Im3d::TextFlags_Default, text.string);

	}

	Im3d::PopDrawState();
}

bool RaytracingRenderer::raycast(const Ray& _in, RayHit& out_)
{
	physx::PxRaycastBuffer queryResult;
	physx::PxHitFlags flags = (physx::PxHitFlags)0
		| physx::PxHitFlag::ePOSITION
		| physx::PxHitFlag::eNORMAL
		//| physx::PxHitFlag::eMESH_ANY
		| physx::PxHitFlag::eFACE_INDEX
		;

	if (!m_impl->pxScene->raycast(Vec3ToPx(_in.origin), Vec3ToPx(_in.direction), _in.maxDistance, queryResult) || !queryResult.hasBlock)
	{
		return false;
	}

	out_.position      = PxToVec3(queryResult.block.position);
	out_.normal        = PxToVec3(queryResult.block.normal);
	out_.distance      = queryResult.block.distance;
	out_.triangleIndex = queryResult.block.faceIndex;

	return true;
}

void RaytracingRenderer::raycast(const Ray* _in, RayHit* out_, size_t _count)
{
	PROFILER_MARKER_CPU("RaytracingRenderer::rayCast");

	m_impl->jobsPerThread.assign(m_impl->jobsPerThread.size(), 0.0f);

	Impl::RayJobList& rayJobs = m_impl->rayJobs;
	rayJobs.raysIn  = _in;
	rayJobs.raysOut = out_;
	rayJobs.count.store((uint32)_count);
	rayJobs.readAt.store(0);

	while (m_impl->processJobs(m_impl->maxJobsPerThrad) != 0)
	{
		std::this_thread::yield();
	}

	// \todo Need to block here until all jobs have unlocked the pxScene. This is a hack to make sure all jobs are finished before we proceed to update the simulation.
	m_impl->pxScene->lockWrite();
	m_impl->pxScene->unlockWrite();
}

void RaytracingRenderer::sortRayHits(RayHit* _hits_, size_t _count)
{
	PROFILER_MARKER_CPU("RaytracingRenderer::sortRayHits");

	auto MakeKey = [](const RayHit& _hit) -> uint64_t
		{
			if (!_hit.instance)
			{
				return 0;
			}
			uint64_t ret = (uint64_t)((Instance*)_hit.instance)->meshData;
			ret = BitfieldInsert(ret, (uint64_t)_hit.isHit, 63, 1);
			return ret;
		};


	FRM_LOG("first=0x%x last=0x%x", _hits_, _hits_ + _count);
	// \todo More efficient to pre-generate the keys?
	eastl::sort(_hits_, _hits_ + _count,
		[MakeKey](const RayHit& _a, const RayHit& _b)
		{
			return MakeKey(_b) < MakeKey(_a);
		});
}

void RaytracingRenderer::bindInstance(GlContext* _ctx_, const void* _instance)
{
	const Instance* instance = (Instance*)_instance;
	const BasicMaterial* material = instance->meshData->material;

	_ctx_->bindBuffer("bfIndexData", instance->meshData->bfIndexData);
	_ctx_->bindBuffer("bfVertexData", instance->meshData->bfVertexData);
	material->bind(_ctx_);
}

// PROTECTED

void RaytracingRenderer::OnNodeShutdown(SceneNode* _node, RaytracingRenderer* _renderer)
{
	BasicRenderableComponent* renderable = (BasicRenderableComponent*)_node->findComponent(StringHash("BasicRenderableComponent"));
	FRM_ASSERT(renderable);
	_renderer->removeInstances(renderable);
}

RaytracingRenderer::~RaytracingRenderer()
{
	shutdown();
}


bool RaytracingRenderer::init(uint32 _threadCount, uint32 _maxRaysPerThread)
{
	shutdown();

	m_impl = FRM_NEW(Impl);

	FRM_ASSERT(g_pxPhysics);
	physx::PxSceneDesc sceneDesc(g_pxPhysics->getTolerancesScale());
	sceneDesc.filterShader = &physx::PxDefaultSimulationFilterShader;
	sceneDesc.cpuDispatcher = g_pxDispatcher;
	m_impl->pxScene = g_pxPhysics->createScene(sceneDesc);
	FRM_ASSERT(m_impl->pxScene);
	m_impl->pxMaterial = g_pxPhysics->createMaterial(1.0f, 1.0f, 0.0f);
	FRM_ASSERT(m_impl->pxMaterial);

	m_impl->pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f);
	m_impl->pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);

	m_impl->maxJobsPerThrad = _maxRaysPerThread;
	m_impl->jobsPerThread.resize(_threadCount);
	for (uint32 threadIndex = 1; threadIndex < _threadCount; ++threadIndex)
	{
		m_impl->threadPool.push_back(std::thread(&Impl::ThreadFunc, m_impl));
	}

	return true;
}

void RaytracingRenderer::shutdown()
{
	if (!m_impl)
	{
		return;
	}

	m_impl->threadShutdown = true;
	for (std::thread& thread : m_impl->threadPool)
	{
		thread.join();
	}
	m_impl->threadPool.clear();
	
	// \hack This is a bit ugly - removeInstances() modifies the map so we can't iterate, instead just keep deleting the first element until empty.
	while (!m_impl->sceneMap.empty())
	{
		removeInstances(m_impl->sceneMap.begin()->first);
	}

	m_impl->pxMaterial->release();
	m_impl->pxScene->release();

	FRM_DELETE(m_impl);
	m_impl = nullptr;
}


bool RaytracingRenderer::addInstances(BasicRenderableComponent* _renderable)
{
	PROFILER_MARKER_CPU("RaytracingRenderer::addInstances");

	bool ret = true;

	FRM_STRICT_ASSERT(m_impl);
	FRM_STRICT_ASSERT(m_impl->sceneMap.find(_renderable) == m_impl->sceneMap.end());

	SceneNode* sceneNode = _renderable->getParentNode();
	const vec3 nodeScale = GetScale(sceneNode->getInitial());
	sceneNode->registerCallback(SceneNode::Event::OnShutdown, (SceneNode::Callback*)&OnNodeShutdown, this);
	sceneNode->registerCallback(SceneNode::Event::OnEdit, (SceneNode::Callback*)&OnNodeShutdown, this);

	// Load mesh. Don't do any additional processing yet, we just need to know the submesh count.
	const PathStr meshPath = _renderable->getMesh()->getPath();
	Mesh* mesh = Mesh::Create(meshPath.c_str(), Mesh::CreateFlags(0));
	FRM_ASSERT(mesh);
	const DateTime meshDate = FileSystem::GetTimeModified(meshPath.c_str());

	// Generate 1 instance per submesh.
	Impl::SceneMap& sceneMap = m_impl->sceneMap;
	Impl::InstanceList& instanceList = sceneMap[_renderable];
	for (size_t submeshIndex = 0; submeshIndex < _renderable->m_materials.size(); ++submeshIndex)
	{
		BasicMaterial* material = _renderable->m_materials[submeshIndex];
		if (!material)
		{
			continue;
		}

		Instance* instance  = m_impl->instancePool.alloc();
		instanceList.push_back(instance);
		instance->sceneNode  = sceneNode;

		uint64_t meshDataKey = MeshData::MakeKey(mesh, material);
		MeshData*& meshData = m_impl->meshDataMap[meshDataKey];
		if (!meshData)
		{
			meshData = m_impl->meshDataPool.alloc();
			meshData->key = meshDataKey;
			meshData->material = material;
			
			File cachedData;
			PathStr cachedPath = PathStr("_cache/%s_%d.raytracing", FileSystem::GetFileName(meshPath.c_str()).c_str(), submeshIndex);
			if (FileSystem::Exists(cachedPath.c_str()))
			{			
				const DateTime cachedDate = FileSystem::GetTimeModified(cachedPath.c_str());
				if (meshDate <= cachedDate)
				{
					FRM_LOG("RaytracingRenderer: Loading cached data '%s'", cachedPath.c_str());
					if (!FileSystem::Read(cachedData, cachedPath.c_str()))
					{
						FRM_LOG_ERR("RaytracingRenderer: Error loading cached data '%s'", cachedPath.c_str());
						continue;
					}
				}
			}
	
			// \todo Since we use the mesh data regardless of whether the pxTrianlgeMesh was cached, need to always process the mesh. This could be avoided by additionally caching the mesh data.
			if (!mesh->getVertexData(Mesh::Semantic_Normals))
			{
				mesh->generateNormals();
			}	
			if (!mesh->getVertexData(Mesh::Semantic_Tangents))
			{
				mesh->generateTangents();
			}
			if (!mesh->getVertexData(Mesh::Semantic_LightmapUVs))
			{
				mesh->generateLightmapUVs();
			}	
			mesh->optimize();
			mesh->computeBounds();

			if (cachedData.getDataSize() == 0)
			{	
				PxInitCooker();
				FRM_ASSERT(g_pxCooking);
	
				physx::PxTriangleMeshDesc meshDesc = {};
				meshDesc.points.count              = (physx::PxU32)mesh->getVertexCount();
				meshDesc.points.stride             = (physx::PxU32)sizeof(vec3); // \todo Verify correct data type.
				meshDesc.points.data               = mesh->getVertexData(Mesh::Semantic_Positions);
				meshDesc.triangles.count           = (physx::PxU32)mesh->getIndexCount(0, submeshIndex) / 3;
				meshDesc.triangles.stride          = (physx::PxU32)DataTypeSizeBytes(mesh->getIndexDataType()) * 3;
				meshDesc.triangles.data            = mesh->getIndexData(0, submeshIndex);
				meshDesc.flags                     = (physx::PxMeshFlags)0; //| physx::PxMeshFlag::eFLIPNORMALS; // use eFLIPNORMALS to flip triangle winding
				if (mesh->getIndexDataType() == DataType_Uint16)
				{
					meshDesc.flags |= physx::PxMeshFlag::e16_BIT_INDICES;
				}
	
				//physx::PxCookingParams cookingParams(g_pxPhysics->getTolerancesScale());
				//cookingParams.meshPreprocessParams.set(physx::PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH);
				//g_pxCooking->setParams(cookingParams);

				physx::PxDefaultMemoryOutputStream pxOutput;
				physx::PxTriangleMeshCookingResult::Enum err = physx::PxTriangleMeshCookingResult::Enum::eSUCCESS;
				if (!g_pxCooking->cookTriangleMesh(meshDesc, pxOutput, &err))
				{
					const char* errStr = "unknown error";
					switch (err)
					{
						default:
							break;
						case physx::PxTriangleMeshCookingResult::Enum::eLARGE_TRIANGLE:
							errStr = "large triangle";
							break;
					};
					FRM_LOG_ERR("PxCookTriangleMesh failed: '%s'", errStr);
					continue;
				}
				cachedData.setData((const char*)pxOutput.getData(), pxOutput.getSize()); // \todo avoid this copy?
				FileSystem::Write(cachedData, cachedPath.c_str());
			}
				
			physx::PxDefaultMemoryInputData pxInput((physx::PxU8*)cachedData.getData(), (physx::PxU32)cachedData.getDataSize() - 1);
			meshData->pxGeometry.triangleMesh() = physx::PxTriangleMeshGeometry(g_pxPhysics->createTriangleMesh(pxInput));
			FRM_ASSERT(meshData->pxGeometry.triangleMesh().triangleMesh->getTrianglesRemap());

			// Copy mesh data into GPU buffers.
			{
				eastl::vector<VertexData> vertexData;
				vertexData.resize(mesh->getVertexCount());
				Mesh::VertexDataView<vec3> normalsView     = mesh->getVertexDataView<vec3>(Mesh::Semantic_Normals);
				Mesh::VertexDataView<vec2> materialUVsView = mesh->getVertexDataView<vec2>(Mesh::Semantic_MaterialUVs);
				Mesh::VertexDataView<vec2> lightmapUVsView = mesh->getVertexDataView<vec2>(Mesh::Semantic_LightmapUVs);
				for (size_t i = 0; i < vertexData.size(); ++i)
				{				
					vertexData[i].materialUV = materialUVsView[i];
					vertexData[i].lightmapUV = lightmapUVsView[i];
					vertexData[i].normal     = normalsView[i];
				}
				meshData->bfVertexData = Buffer::Create(GL_SHADER_STORAGE_BUFFER, (GLsizei)(sizeof(VertexData) * vertexData.size()), 0, vertexData.data());
			}
			{
				FRM_ASSERT(mesh->getIndexDataType() == DataType_Uint32); // \todo Convert 16 bit indices.
				meshData->bfIndexData = Buffer::Create(GL_SHADER_STORAGE_BUFFER, (GLsizei)(sizeof(uint32) * mesh->getIndexCount(0, submeshIndex)), 0, mesh->getIndexData(0, submeshIndex));
			}
		}
		
		instance->meshData = meshData;
		++meshData->refCount;

		// Init pxShape, pxRigidActor.		
		meshData->pxGeometry.triangleMesh().scale = physx::PxMeshScale(Vec3ToPx(nodeScale));
		instance->pxShape = g_pxPhysics->createShape(meshData->pxGeometry.any(), *m_impl->pxMaterial, true, physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE);

		instance->pxRigidActor = g_pxPhysics->createRigidStatic(Mat4ToPxTransform(sceneNode->getInitial()));
		instance->pxRigidActor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true); // \todo enable/disable for all actors when toggling debug draw?
		instance->pxRigidActor->userData = instance;
		instance->pxRigidActor->attachShape(*instance->pxShape);

		m_impl->pxScene->addActor(*instance->pxRigidActor);
	}

	Mesh::Destroy(mesh);

	return ret;
}

void RaytracingRenderer::removeInstances(BasicRenderableComponent* _renderable)
{
	PROFILER_MARKER_CPU("RaytracingRenderer::removeInstances");

	auto it = m_impl->sceneMap.find(_renderable);
	if (it == m_impl->sceneMap.end())
	{
		return;
	}
	
	_renderable->getParentNode()->unregisterCallback(SceneNode::Event::OnShutdown, (SceneNode::Callback*)&OnNodeShutdown, this);
	_renderable->getParentNode()->unregisterCallback(SceneNode::Event::OnEdit, (SceneNode::Callback*)&OnNodeShutdown, this);

	Impl::InstanceList& instanceList = it->second;
	for (Instance* instance : instanceList)
	{
		m_impl->pxScene->removeActor(*instance->pxRigidActor);
		instance->pxRigidActor->detachShape(*instance->pxShape, false);
		instance->pxShape->release();
		instance->pxRigidActor->release();

		MeshData* meshData = instance->meshData;
		FRM_ASSERT(meshData->refCount > 0);
		--meshData->refCount;
		if (meshData->refCount == 0)
		{
			meshData->pxGeometry.triangleMesh().triangleMesh->release();
			m_impl->meshDataMap.erase(meshData->key);
			Buffer::Destroy(meshData->bfVertexData);
			Buffer::Destroy(meshData->bfIndexData);
			m_impl->meshDataPool.free(meshData);
		}
		m_impl->instancePool.free(instance);
	}
	m_impl->sceneMap.erase(it);
}

} // namespace

#endif // FRM_MODULE_PHYSICS
