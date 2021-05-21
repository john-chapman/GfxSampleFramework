#include "Mesh.h"

#include <frm/core/log.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/SkeletonAnimation.h>
#include <frm/core/Time.h>

#include <gltf.h>
#include <string>

namespace frm {

bool Mesh::ReadGLTF(Mesh& mesh_, const char* _srcData, size_t _srcDataSizeBytes, CreateFlags _createFlags)
{
	FRM_AUTOTIMER("Mesh::ReadGLTF");

	tinygltf::Model gltf;
	{	FRM_AUTOTIMER("Parse GLTF");
		const PathStr rootPath = FileSystem::GetPath(mesh_.getPath());
		if (!tinygltf::Load(_srcData, _srcDataSizeBytes, rootPath.c_str(), gltf))
		{
			return false;
		}
	}

	// We discard the actual submesh hierarchy here and generate a single submesh per material ID. Skeletons ("skins" in gltf terms) are also merged.
	eastl::vector<Mesh> meshPerMaterial;
	meshPerMaterial.resize(Max((size_t)1, gltf.materials.size()));

	Skeleton skeleton;
	eastl::vector<int> boneIndexMap(gltf.nodes.size(), -1);
	eastl::vector<mat4> bindPose;

	bool generateNormals = false;
	bool generateTangents = false;
	bool generateLightmapUVs = false;
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
				const int thisNodeIndex = nodeStack.back();
				const auto& node = gltf.nodes[thisNodeIndex];
				nodeStack.pop_back();
				const mat4 transform = transformStack.back();
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
					const mat4 childTransform = GetTransform(&gltf.nodes[nodeStack.back()]);
					transformStack.push_back(transform * childTransform);
				}

				if (node.mesh == -1)
				{
					continue;
				}

				if (node.skin != -1)
				{
					const tinygltf::Skin& skin = gltf.skins[node.skin];
					FRM_VERIFY(tinygltf::LoadSkeleton(gltf, skin, boneIndexMap, skeleton));

					tinygltf::AutoAccessor bindPoseAccessor(gltf.accessors[skin.inverseBindMatrices], gltf);
					do
					{
						bindPose.push_back(bindPoseAccessor.get<mat4>());
					} while (bindPoseAccessor.next());
					FRM_ASSERT(bindPose.size() == skeleton.getBoneCount());
				}

				auto& mesh = gltf.meshes[node.mesh];
				for (auto& meshPrimitive : mesh.primitives)
				{
					if (meshPrimitive.mode != TINYGLTF_MODE_TRIANGLES) // only triangles are supported
					{
						continue;
					}
					
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
					Mesh& submesh = meshPerMaterial[Max(0, meshPrimitive.material)];
					const uint32 vertexOffset = submesh.getVertexCount();
					submesh.setVertexCount(vertexOffset + vertexCount);
					{	
						VertexDataView<vec3> positionsDst = submesh.getVertexDataView<vec3>(Mesh::Semantic_Positions, vertexOffset);
						FRM_ASSERT(positionsDst.getCount() == vertexCount);
						const auto& bufferView = gltf.bufferViews[positionsAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + positionsAccessor.byteOffset;
						const size_t stride = positionsAccessor.ByteStride(bufferView);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex, buffer += stride)
						{
							positionsDst[vertexIndex] = TransformPosition(transform, *((vec3*)buffer));
						}
					}

					FRM_ASSERT(indicesAccessor.count % 3 == 0);
					const uint32 indexCount = (uint32)indicesAccessor.count;
					const uint32 triangleCount = indexCount / 3;
					const uint32 indexOffset = submesh.m_lods[0].submeshes[0].indexCount;
					submesh.setIndexData(0, DataType_Uint32, indexOffset + indexCount);
					IndexDataView<uint32> indexDst = submesh.getIndexDataView<uint32>(0, 0, indexOffset);					
					{
						const auto& bufferView = gltf.bufferViews[indicesAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + indicesAccessor.byteOffset;
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
						VertexDataView<vec3> normalsDst = submesh.getVertexDataView<vec3>(Mesh::Semantic_Normals, vertexOffset);
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["NORMAL"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{
							normalsDst[vertexIndex] = TransformDirection(transform, Normalize(accessor.get<vec3>()));
							accessor.next();
						}
					}
					else
					{
						generateNormals = generateTangents = true;
					}

					if (meshPrimitive.attributes.find("TANGENT") != meshPrimitive.attributes.end())
					{
						VertexDataView<vec4> tangentsDst = submesh.getVertexDataView<vec4>(Mesh::Semantic_Tangents, vertexOffset);
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["TANGENT"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{
							vec4 t = accessor.get<vec4>();
							tangentsDst[vertexIndex] = vec4(TransformDirection(transform, Normalize(t.xyz())), t.w);
							accessor.next();
						}
					}
					else
					{
						generateTangents = true;
					}

					if (meshPrimitive.attributes.find("TEXCOORD_0") != meshPrimitive.attributes.end())
					{
						VertexDataView<vec2> dst = submesh.getVertexDataView<vec2>(Mesh::Semantic_MaterialUVs, vertexOffset);
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["TEXCOORD_0"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{
							dst[vertexIndex] = accessor.get<vec2>();
							accessor.next();
						}
					}

					if (meshPrimitive.attributes.find("TEXCOORD_1") != meshPrimitive.attributes.end())
					{
						VertexDataView<vec2> dst = submesh.getVertexDataView<vec2>(Mesh::Semantic_LightmapUVs, vertexOffset);
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["TEXCOORD_1"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{
							dst[vertexIndex] = accessor.get<vec2>();
							accessor.next();
						}
					}
					else
					{
						generateLightmapUVs = true;
					}

					// Note that we don't require the bone index map here, indices are already relative to the skin joints list.
					if (meshPrimitive.attributes.find("JOINTS_0") != meshPrimitive.attributes.end())
					{
						hasBoneIndices = true;

						VertexDataView<uvec4> boneIndicesDst = submesh.getVertexDataView<uvec4>(Mesh::Semantic_BoneIndices, vertexOffset);
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["JOINTS_0"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{
							boneIndicesDst[vertexIndex] = accessor.get<uvec4>();
							accessor.next();
						}
					}					

					if (meshPrimitive.attributes.find("WEIGHTS_0") != meshPrimitive.attributes.end())
					{
						hasBoneWeights = true;

						VertexDataView<vec4> boneWeightsDst = submesh.getVertexDataView<vec4>(Mesh::Semantic_BoneWeights, vertexOffset);
						tinygltf::AutoAccessor accessor(gltf.accessors[meshPrimitive.attributes["WEIGHTS_0"]], gltf);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
						{
							boneWeightsDst[vertexIndex] = accessor.get<vec4>();
							accessor.next();
						}
					}
				}
			}
		}
	}

	Mesh finalMesh;
	finalMesh.m_path = mesh_.m_path;

	{	FRM_AUTOTIMER("Compile submeshes");
	
		for (Mesh& submesh : meshPerMaterial)
		{
			finalMesh.addSubmesh(0, submesh);
		}
		meshPerMaterial.clear();

		// \todo \hack In this case the second submesh is redundant. We always create submesh 0 to represent the whole mesh, in this case there was only 1 per material submesh and so it also represents the whole mesh.
		if (finalMesh.m_lods[0].submeshes.size() == 2)
		{
			finalMesh.m_lods[0].submeshes.pop_back();
		}

		if (!bindPose.empty())
		{
			skeleton.setPose(bindPose.data());
			finalMesh.setSkeleton(skeleton);
		}
	}

	{	FRM_AUTOTIMER("Finalize");

		if (generateNormals && _createFlags.get(CreateFlag::GenerateNormals))
		{
			finalMesh.generateNormals();
		}

		if (generateTangents && _createFlags.get(CreateFlag::GenerateTangents))
		{
			finalMesh.generateTangents();
		}

		if (generateLightmapUVs && _createFlags.get(CreateFlag::GenerateLightmapUVs))
		{
			finalMesh.generateLightmapUVs();
		}

		if (_createFlags.get(CreateFlag::Optimize))
		{
			finalMesh.optimize();
		}

		if (_createFlags.get(CreateFlag::GenerateLODs))
		{
			finalMesh.generateLODs(5, 0.6f, 0.1f);
		}

		finalMesh.computeBounds();
	}

	mesh_.unload();
	mesh_.swap(finalMesh);

	return true;
}

} // namespace frm