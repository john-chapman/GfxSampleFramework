#include "MeshData.h"

#include <frm/core/log.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Time.h>

#include <gltf.h>
#include <string>

namespace frm {

bool MeshData::ReadGltf(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{
	tinygltf::Model gltf;
	const PathStr rootPath = FileSystem::GetPath(mesh_.getPath());
	if (!tinygltf::Load(_srcData, _srcDataSize, rootPath.c_str(), gltf))
	{
		return false;
	}
	
	// We discard the actual submesh hierarchy here and generate a single submesh per material ID. Skeletons ("skins" in gltf terms) are also merged.
	eastl::vector<MeshBuilder> meshBuilderPerMaterial;
	meshBuilderPerMaterial.resize(Max((size_t)1, gltf.materials.size()));
	eastl::vector<mat4> bindPose;

	bool generateNormals = false;
	bool generateTangents = false;
	bool hasBoneIndices = false;
	bool hasBoneWeights = false;

	for (auto& scene : gltf.scenes)
	{
		if (scene.nodes.empty())
		{
			continue;
		}
			
		// We traverse the scene's node list *and* recursively traverse each node's subtree. This may cause nodes to be visited multiple times if the scene node hierarchy isn't well formed.
		eastl::vector<int> visitedNodes;

		for (auto nodeIndex : scene.nodes)
		{
			std::vector<int> nodeStack;
			std::vector<mat4> transformStack;
			nodeStack.push_back(nodeIndex);
			transformStack.push_back(GetTransform(&gltf.nodes[nodeStack.back()]));
			
			while (!nodeStack.empty())
			{
				int thisNodeIndex = nodeStack.back();
				const auto& node = gltf.nodes[thisNodeIndex];
				nodeStack.pop_back();
				auto transform = transformStack.back();
				transformStack.pop_back();

				if (eastl::find(visitedNodes.begin(), visitedNodes.end(), thisNodeIndex) != visitedNodes.end())
				{
					FRM_LOG_ERR("Warning: Node hierarchy is not well-formed");
					continue;
				}
				else
				{
					visitedNodes.push_back(thisNodeIndex);
				}

				for (auto childIndex : node.children)
				{
					nodeStack.push_back(childIndex);
					mat4 childTransform = GetTransform(&gltf.nodes[nodeStack.back()]);
					transformStack.push_back(transform * childTransform);
				}

				if (node.mesh == -1)
				{
					continue;
				}

				eastl::vector<int> boneIndexMap(gltf.nodes.size(), -1); // Map node indices -> bone indices.
				if (node.skin != -1)
				{
					const auto& skin = gltf.skins[node.skin];

					tinygltf::Accessor bindPoseAccessor = gltf.accessors[skin.inverseBindMatrices];
					FRM_ASSERT(bindPoseAccessor.count == skin.joints.size());
					FRM_ASSERT(bindPoseAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
					FRM_ASSERT(bindPoseAccessor.type == TINYGLTF_TYPE_MAT4);					
					const auto& bufferView = gltf.bufferViews[bindPoseAccessor.bufferView];
					const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + bindPoseAccessor.byteOffset;
					const size_t stride = bindPoseAccessor.ByteStride(bufferView);
					for (int boneIndex = 0; boneIndex < (int)bindPoseAccessor.count; ++boneIndex, buffer += stride)
					{
						const mat4 transform = tinygltf::GetMatrix((const float*)buffer);
						boneIndexMap[skin.joints[boneIndex]] = (int)bindPose.size();
						bindPose.push_back(transform);
					}
				}

				auto& mesh = gltf.meshes[node.mesh];
				for (auto& meshPrimitive : mesh.primitives)
				{
					if (meshPrimitive.mode != TINYGLTF_MODE_TRIANGLES) // only triangles are supported
					{
						continue;
					}
				
					MeshBuilder& meshBuilder = meshBuilderPerMaterial[Max(0, meshPrimitive.material)];
					const uint32 vertexOffset = meshBuilder.getVertexCount();

					tinygltf::Accessor positionsAccessor;
					if (meshPrimitive.attributes.find("POSITION") != meshPrimitive.attributes.end())
					{
						positionsAccessor = gltf.accessors[meshPrimitive.attributes["POSITION"]];
						FRM_ASSERT(positionsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
						FRM_ASSERT(positionsAccessor.type == TINYGLTF_TYPE_VEC3);
					}
					else
					{
						FRM_LOG_ERR("Mesh '%s' contains no vertex positions", mesh.name.c_str());
						continue;
					}

					tinygltf::Accessor indicesAccessor;
					if (meshPrimitive.indices >= 0)
					{
						indicesAccessor = gltf.accessors[meshPrimitive.indices];
					}
					else
					{
						FRM_LOG_ERR("Mesh '%s' contains no indices", mesh.name.c_str());
						continue;
					}

					const uint32 vertexCount = (uint32)positionsAccessor.count;
					{	
						const auto& bufferView = gltf.bufferViews[positionsAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + positionsAccessor.byteOffset;
						const size_t stride = positionsAccessor.ByteStride(bufferView);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex, buffer += stride)
						{
							MeshBuilder::Vertex vertex;
							vertex.m_position = TransformPosition(transform, *((vec3*)buffer));
							meshBuilder.addVertex(vertex);
						}
					}

					FRM_ASSERT(indicesAccessor.count % 3 == 0);
					const uint32 triangleCount = (uint32)indicesAccessor.count / 3;
					{
						const auto& bufferView = gltf.bufferViews[indicesAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + indicesAccessor.byteOffset;
						const size_t stride = indicesAccessor.ByteStride(bufferView);
						for (uint32 triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
						{
							MeshBuilder::Triangle triangle;
							for (uint32 vertexIndex = 0; vertexIndex < 3; ++vertexIndex, buffer += stride)
							{
								switch (indicesAccessor.componentType)
								{
									default: 
										FRM_ASSERT(false);
									case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
										triangle[vertexIndex] = (uint32)*((unsigned int*)buffer);
										break;
									case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
										triangle[vertexIndex] = (uint32)*((unsigned short*)buffer);
										break;
								};
								triangle[vertexIndex] += vertexOffset;
							}
							meshBuilder.addTriangle(triangle);
						}
					}

					if (meshPrimitive.attributes.find("NORMAL") != meshPrimitive.attributes.end())
					{
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["NORMAL"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{							
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vertex.m_normal = TransformDirection(transform, Normalize(accessor.get<vec3>()));
							FRM_VERIFY(accessor.next());
						}
					}
					else
					{
						generateNormals = generateTangents = true;
					}

					if (meshPrimitive.attributes.find("TANGENT") != meshPrimitive.attributes.end())
					{
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["TANGENT"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{							
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vec4 t = accessor.get<vec4>();
							vertex.m_tangent = vec4(TransformDirection(transform, Normalize(vertex.m_tangent.xyz())), vertex.m_tangent.w);
							FRM_VERIFY(accessor.next());
						}
					}
					else
					{
						generateTangents = true;
					}

					if (meshPrimitive.attributes.find("TEXCOORD_0") != meshPrimitive.attributes.end())
					{
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["TEXCOORD_0"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{							
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vertex.m_texcoord = accessor.get<vec2>();
							FRM_VERIFY(accessor.next());
						}
					}

					if (meshPrimitive.attributes.find("JOINTS_0") != meshPrimitive.attributes.end())
					{
						hasBoneIndices = true;

						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["JOINTS_0"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{							
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vertex.m_boneIndices = accessor.get<uvec4>();
							FRM_VERIFY(accessor.next());
						}
					}					

					if (meshPrimitive.attributes.find("WEIGHTS_0") != meshPrimitive.attributes.end())
					{
						hasBoneWeights = true;

						tinygltf::AutoAccessor boneWeightsAccessor(gltf.accessors[meshPrimitive.attributes["WEIGHTS_0"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{							
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vertex.m_boneWeights = boneWeightsAccessor.get<vec4>();
							FRM_VERIFY(boneWeightsAccessor.next());
						}
					}
				}
			}
		}
	}

	MeshBuilder finalMeshBuilder;
	for (uint materialId = 0; materialId < meshBuilderPerMaterial.size(); ++materialId)
	{
		finalMeshBuilder.beginSubmesh(materialId);
		finalMeshBuilder.addMesh(meshBuilderPerMaterial[materialId]);
		finalMeshBuilder.endSubmesh();
	}

	MeshDesc meshDesc = mesh_.getDesc();

	if (meshDesc.findVertexAttr(VertexAttr::Semantic_Normals) && generateNormals)
	{
		FRM_AUTOTIMER("Generate normals");
		finalMeshBuilder.generateNormals();
	}

	if (meshDesc.findVertexAttr(VertexAttr::Semantic_Tangents) && generateTangents)
	{
		FRM_AUTOTIMER("Generate tangents");
		finalMeshBuilder.generateTangents();
	}

	if (!meshDesc.findVertexAttr(VertexAttr::Semantic_BoneWeights) && hasBoneWeights)
	{
		meshDesc.addVertexAttr(VertexAttr::Semantic_BoneWeights, DataType_Float, 4);
	}

	if (!meshDesc.findVertexAttr(VertexAttr::Semantic_BoneIndices) && hasBoneIndices)
	{
		DataType indexType = DataType_Uint16;
		if (bindPose.size() < 256)
		{
			indexType = DataType_Uint8;
		}
		meshDesc.addVertexAttr(VertexAttr::Semantic_BoneIndices, indexType, 4);
	}

	MeshData retMesh(meshDesc, finalMeshBuilder);
	swap(mesh_, retMesh);

	mesh_.setBindPose(bindPose.data(), bindPose.size());

	return true;
}

} // namespace frm
