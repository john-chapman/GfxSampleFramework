#include "Mesh.h"

#include <frm/core/log.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/SkeletonAnimation.h>
#include <frm/core/Time.h>

#include <gltf.h>
#include <string>

static eastl::vector<int> FilterRootNodes(tinygltf::Model& _gltf, std::initializer_list<const char*> _filters)
{
	auto Filter = [_filters](const char* _name) -> bool
		{
			for (const char* filter : _filters)
			{
				if (_stricmp(filter, _name) == 0)
				{
					return true;
				}
			}

			return false;
		};

	eastl::vector<int> filteredRootNodes;
	if (_filters.size() == 0)
	{
		//filteredRootNodes.assign(_gltf.scenes[0].nodes.begin(), _gltf.scenes[0].nodes.end());
		filteredRootNodes.reserve(_gltf.scenes[0].nodes.size());
		for (int sceneNode : _gltf.scenes[0].nodes)
		{
			filteredRootNodes.push_back(sceneNode);
		}
	}
	else
	{	
		for (auto nodeIndex : _gltf.scenes[0].nodes)
		{
			if (Filter(_gltf.nodes[nodeIndex].name.c_str()))
			{
				filteredRootNodes.push_back(nodeIndex);
			}
		}
	}

	return filteredRootNodes;
}

static eastl::vector<int> FindLODNodes(tinygltf::Model& _gltf, const eastl::vector<int>& _filteredRootNodes, int _searchDepth = 0)
{
	eastl::vector<int> nodePerLOD;

	auto AddLODNode = [&nodePerLOD](int _lodIndex, int _nodeIndex)
		{
			nodePerLOD.resize(frm::Max(nodePerLOD.size(), (size_t)_lodIndex + 1), -1); // -1 = empty slot, see below
			
			if (nodePerLOD[_lodIndex] != -1)
			{
				FRM_LOG_ERR("Warning: Multiple selected nodes contain LOD%d.", _lodIndex);
			}
			nodePerLOD[_lodIndex] = _nodeIndex;
		};

	// Search root nodes for 'LODn'.
	for (auto nodeIndex : _filteredRootNodes)
	{
		const auto& node = _gltf.nodes[nodeIndex];
		const char* nodeName = node.name.c_str();
		if (_strnicmp("LOD", nodeName, 3) == 0)
		{
			const long lodIndex = strtol(nodeName + 3, 0, 0);
			AddLODNode(lodIndex, nodeIndex);
		}
	}

	// Didn't find any LODn nodes, recursive search.
	if (nodePerLOD.empty())
	{
		for (auto nodeIndex : _filteredRootNodes)
		{
			const auto& node = _gltf.nodes[nodeIndex];

			eastl::vector<int> childNodes;
			//childNodes.assign(node.children.begin(), node.children.end());
			childNodes.reserve(node.children.size());
			for (int child : node.children)
			{
				childNodes.push_back(child);
			}

			eastl::vector<int> childLODNodes = FindLODNodes(_gltf, childNodes, _searchDepth + 1);
			for (int lodIndex = 0; lodIndex < (int)childLODNodes.size(); ++lodIndex)
			{
				AddLODNode(lodIndex, childLODNodes[lodIndex]);
			}
		}
	}

	// Still nothing after full recursive search, load the first filtered root only.
	if (nodePerLOD.empty() && _searchDepth == 0)
	{
		nodePerLOD.push_back(_filteredRootNodes.front());
	}

	// Remove empty slots.
	if (_searchDepth == 0)
	{
		for (auto it = nodePerLOD.begin(); it != nodePerLOD.end(); )
		{
			if (*it == -1)
			{
				it = nodePerLOD.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	return nodePerLOD;
}

static bool LoadMesh(tinygltf::Model& _gltf, int _rootNodeIndex, frm::Mesh& mesh_)
{
	using namespace frm;

	eastl::vector<int> visitedNodes; // Sanity check; make sure we visit nodes only once.
	
	eastl::vector<frm::Mesh> meshPerMaterial;
	meshPerMaterial.resize(Max((size_t)1, _gltf.materials.size()));

	frm::Skeleton skeleton;
	eastl::vector<int> boneIndexMap(_gltf.nodes.size(), -1); // Map node indices -> bone indices in the output skeleton.
	eastl::vector<mat4> bindPose;

	std::vector<int>  nodeStack;
	std::vector<mat4> transformStack;
	nodeStack.push_back(_rootNodeIndex);
	transformStack.push_back(identity); // Discard root transform. This allows multi-mesh source files to be more conveniently arranged.
		
	while (!nodeStack.empty())
	{
		const int thisNodeIndex = nodeStack.back();
		const auto& node = _gltf.nodes[thisNodeIndex];
		nodeStack.pop_back();
		const mat4 transform = transformStack.back();
		transformStack.pop_back();

		if (eastl::find(visitedNodes.begin(), visitedNodes.end(), thisNodeIndex) != visitedNodes.end())
		{
			FRM_LOG_ERR("Warning: Node hierarchy is not well-formed.");
			continue;
		}
		else
		{
			visitedNodes.push_back(thisNodeIndex);
		}

		for (auto childIndex : node.children)
		{
			nodeStack.push_back(childIndex);
			const mat4 childTransform = GetTransform(&_gltf.nodes[nodeStack.back()]);
			transformStack.push_back(transform * childTransform);
		}

		if (node.mesh == -1)
		{
			continue;
		}

		if (node.skin != -1)
		{
			const tinygltf::Skin& skin = _gltf.skins[node.skin];
			FRM_VERIFY(tinygltf::LoadSkeleton(_gltf, skin, boneIndexMap, skeleton));

			tinygltf::AutoAccessor bindPoseAccessor(_gltf.accessors[skin.inverseBindMatrices], _gltf);
			do
			{
				bindPose.push_back(bindPoseAccessor.get<mat4>());
			} while (bindPoseAccessor.next());
			FRM_ASSERT(bindPose.size() == skeleton.getBoneCount());
		}

		auto& mesh = _gltf.meshes[node.mesh];
		for (auto& meshPrimitive : mesh.primitives)
		{
			if (meshPrimitive.mode != TINYGLTF_MODE_TRIANGLES) // only triangles are supported
			{
				continue;
			}
			
			tinygltf::Accessor positionsAccessor;
			if (meshPrimitive.attributes.find("POSITION") != meshPrimitive.attributes.end())
			{
				positionsAccessor = _gltf.accessors[meshPrimitive.attributes["POSITION"]];
				FRM_ASSERT(positionsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				FRM_ASSERT(positionsAccessor.type == TINYGLTF_TYPE_VEC3);
			}
			else
			{
				FRM_LOG_ERR("Mesh '%s' contains no vertex positions.", mesh.name.c_str());
				continue;
			}

			tinygltf::Accessor indicesAccessor;
			if (meshPrimitive.indices >= 0)
			{
				indicesAccessor = _gltf.accessors[meshPrimitive.indices];
			}
			else
			{
				FRM_LOG_ERR("Mesh '%s' contains no indices.", mesh.name.c_str());
				continue;
			}

			const uint32 vertexCount = (uint32)positionsAccessor.count;
			Mesh& submesh = meshPerMaterial[Max(0, meshPrimitive.material)];
			const uint32 vertexOffset = submesh.getVertexCount();
			submesh.setVertexCount(vertexOffset + vertexCount);
			{	
				Mesh::VertexDataView<vec3> positionsDst = submesh.getVertexDataView<vec3>(Mesh::Semantic_Positions, vertexOffset);
				FRM_ASSERT(positionsDst.getCount() == vertexCount);
				const auto& bufferView = _gltf.bufferViews[positionsAccessor.bufferView];
				const unsigned char* buffer = _gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + positionsAccessor.byteOffset;
				const size_t stride = positionsAccessor.ByteStride(bufferView);
				for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex, buffer += stride)
				{
					positionsDst[vertexIndex] = TransformPosition(transform, *((vec3*)buffer));
				}
			}

			FRM_ASSERT(indicesAccessor.count % 3 == 0);
			const uint32 indexCount = (uint32)indicesAccessor.count;
			const uint32 triangleCount = indexCount / 3;
			const uint32 indexOffset = submesh.getIndexCount();			
			submesh.setIndexData(0, DataType_Uint32, indexOffset + indexCount);
			Mesh::IndexDataView<uint32> indexDst = submesh.getIndexDataView<uint32>(0, 0, indexOffset);					
			{
				const auto& bufferView = _gltf.bufferViews[indicesAccessor.bufferView];
				const unsigned char* buffer = _gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + indicesAccessor.byteOffset;
				const size_t stride = indicesAccessor.ByteStride(bufferView);
				for (uint32 i = 0; i < indexCount; ++i, buffer += stride)
				{
					switch (indicesAccessor.componentType)
					{
						default: 
							FRM_ASSERT(false);
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
							indexDst[i] = (uint32)*((unsigned int*)buffer);
							break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
							indexDst[i] = (uint32)*((unsigned short*)buffer);
							break;
					};
					indexDst[i] += vertexOffset;
				}
			}

			if (meshPrimitive.attributes.find("NORMAL") != meshPrimitive.attributes.end())
			{
				Mesh::VertexDataView<vec3> normalsDst = submesh.getVertexDataView<vec3>(Mesh::Semantic_Normals, vertexOffset);
				tinygltf::AutoAccessor accessor(_gltf.accessors[meshPrimitive.attributes["NORMAL"]], _gltf);
				for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
				{
					normalsDst[vertexIndex] = TransformDirection(transform, Normalize(accessor.get<vec3>()));
					accessor.next();
				}
			}

			if (meshPrimitive.attributes.find("TANGENT") != meshPrimitive.attributes.end())
			{
				Mesh::VertexDataView<vec4> tangentsDst = submesh.getVertexDataView<vec4>(Mesh::Semantic_Tangents, vertexOffset);
				tinygltf::AutoAccessor accessor(_gltf.accessors[meshPrimitive.attributes["TANGENT"]], _gltf);
				for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
				{
					vec4 t = accessor.get<vec4>();
					tangentsDst[vertexIndex] = vec4(TransformDirection(transform, Normalize(t.xyz())), t.w);
					accessor.next();
				}
			}

			if (meshPrimitive.attributes.find("TEXCOORD_0") != meshPrimitive.attributes.end())
			{
				Mesh::VertexDataView<vec2> dst = submesh.getVertexDataView<vec2>(Mesh::Semantic_MaterialUVs, vertexOffset);
				tinygltf::AutoAccessor accessor(_gltf.accessors[meshPrimitive.attributes["TEXCOORD_0"]], _gltf);
				for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
				{
					dst[vertexIndex] = accessor.get<vec2>();
					accessor.next();
				}
			}

			if (meshPrimitive.attributes.find("TEXCOORD_1") != meshPrimitive.attributes.end())
			{
				Mesh::VertexDataView<vec2> dst = submesh.getVertexDataView<vec2>(Mesh::Semantic_LightmapUVs, vertexOffset);
				tinygltf::AutoAccessor accessor(_gltf.accessors[meshPrimitive.attributes["TEXCOORD_1"]], _gltf);
				for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
				{
					dst[vertexIndex] = accessor.get<vec2>();
					accessor.next();
				}
			}

			// Note that we don't require the bone index map here, indices are already relative to the skin joints list.
			if (meshPrimitive.attributes.find("JOINTS_0") != meshPrimitive.attributes.end())
			{
				Mesh::VertexDataView<uvec4> boneIndicesDst = submesh.getVertexDataView<uvec4>(Mesh::Semantic_BoneIndices, vertexOffset);
				tinygltf::AutoAccessor accessor(_gltf.accessors[meshPrimitive.attributes["JOINTS_0"]], _gltf);
				for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
				{
					boneIndicesDst[vertexIndex] = accessor.get<uvec4>();
					accessor.next();
				}
			}					

			if (meshPrimitive.attributes.find("WEIGHTS_0") != meshPrimitive.attributes.end())
			{
				Mesh::VertexDataView<vec4> boneWeightsDst = submesh.getVertexDataView<vec4>(Mesh::Semantic_BoneWeights, vertexOffset);
				tinygltf::AutoAccessor accessor(_gltf.accessors[meshPrimitive.attributes["WEIGHTS_0"]], _gltf);
				for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
				{
					boneWeightsDst[vertexIndex] = accessor.get<vec4>();
					accessor.next();
				}
			}
		}
	}

	for (Mesh& submesh : meshPerMaterial)
	{
		mesh_.addSubmesh(0, submesh);
	}
	meshPerMaterial.clear();

	if (!bindPose.empty())
	{
		skeleton.setPose(bindPose.data());
		mesh_.setSkeleton(skeleton);
	}

	return true;
}


namespace frm {

bool Mesh::ReadGLTF(Mesh& mesh_, const char* _srcData, size_t _srcDataSizeBytes, CreateFlags _createFlags, std::initializer_list<const char*> _filters)
{
	String<64> filterStr;
	for (const char* filter : _filters)
	{
		filterStr.append(filter);
		filterStr.append(", ");
	}

	FRM_AUTOTIMER("Mesh::ReadGLTF (%s)", filterStr.c_str());

	tinygltf::Model gltf;
	{	
		FRM_AUTOTIMER("Parse GLTF");

		const PathStr rootPath = FileSystem::GetPath(mesh_.getPath());
		if (!tinygltf::Load(_srcData, _srcDataSizeBytes, rootPath.c_str(), gltf))
		{
			return false;
		}
	}

	if (gltf.scenes.size() > 1)
	{
		FRM_LOG_ERR("Warning: GLTF contained multiple scenes.");
	}

	// Filter root nodes.
	eastl::vector<int> filteredRootNodes = FilterRootNodes(gltf, _filters);
	
	// \hack If no nodes passed the filter and there is only 1 node, load the whole file.
	if (filteredRootNodes.empty() && gltf.nodes.size() == 1)
	{
		filteredRootNodes.push_back(0);
	}

	if (filteredRootNodes.empty())
	{
		FRM_LOG_ERR("No nodes passed filter list.");
		return false;
	}

	// Determine which nodes to visit per LOD. 
	eastl::vector<int> nodePerLOD = FindLODNodes(gltf, filteredRootNodes);

	// Only load LOD0 if CreateFlag::GenerateLODs is not set.
	while (!_createFlags.get(CreateFlag::GenerateLODs) && nodePerLOD.size() > 1)
	{
		nodePerLOD.pop_back();
	}

	Mesh finalMesh;
	finalMesh.m_path = mesh_.m_path;

	for (int lodIndex = 0; lodIndex < (int)nodePerLOD.size(); ++lodIndex)
	{
		Mesh lodMesh;		
		if (LoadMesh(gltf, nodePerLOD[lodIndex], lodMesh))
		{
			// \todo \hack In this case the second submesh is redundant. We always create submesh 0 to represent the whole mesh, in this case there was only 1 per material submesh and so it also represents the whole mesh.
			if (lodMesh.m_lods[0].submeshes.size() == 2)
			{
				lodMesh.m_lods[0].submeshes.pop_back();
			}

			const bool generateNormals = lodMesh.getVertexData(Semantic_Normals) == nullptr;
			if (generateNormals && _createFlags.get(CreateFlag::GenerateNormals))
			{
				lodMesh.generateNormals();
			}

			const bool generateTangents = generateNormals || lodMesh.getVertexData(Semantic_Tangents) == nullptr;
			if (generateTangents && _createFlags.get(CreateFlag::GenerateNormals))
			{
				lodMesh.generateTangents();
			}			

			if (_createFlags.get(CreateFlag::Optimize))
			{
				lodMesh.optimize();
			}

			finalMesh.addLOD(lodMesh);
		}
	}

	// \todo Only generate lightmap UVs for LOD0. This assumes that other LODs share vertex data with LOD0 hence need to merge instead of appending LODs.
	const bool generateLightmapUVs = finalMesh.getVertexData(Semantic_LightmapUVs) == nullptr;
	if (generateLightmapUVs && _createFlags.get(CreateFlag::GenerateLightmapUVs))
	{
		finalMesh.generateLightmapUVs();
	}

	if (finalMesh.m_lods.size() == 1 && _createFlags.get(CreateFlag::GenerateLODs))
	{
		finalMesh.generateLODs(5, 0.6f, 0.1f);
	}

	finalMesh.computeBounds();

	mesh_.unload();
	mesh_.swap(finalMesh);

	return true;
}

} // namespace frm