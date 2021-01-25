#include "MeshData.h"

#include <frm/core/log.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Time.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
//#define TINYGLTF_NO_FS
#include <tiny_gltf.h>
#include <string>

using namespace frm;

static bool FileExistsFunction(const std::string &abs_filename, void*)
{
	return FileSystem::Exists(abs_filename.c_str());
}

static std::string ExpandFilePathFunction(const std::string& path, void* meshData)
{
	// assume any internal URIs are relative to the source .gltf file
	PathStr meshPath = FileSystem::GetPath(((MeshData*)meshData)->getPath());
	meshPath.appendf("/%s", path.c_str());
	return meshPath.c_str();
}

static bool ReadWholeFileFunction(std::vector<unsigned char>* out, std::string* err, const std::string& filepath, void*)
{
	File f;
	if (!FileSystem::ReadIfExists(f, filepath.c_str()))
	{
		return false;
	}

	const unsigned char* data = (unsigned char*)f.getData();
	const size_t dataSize = (size_t)f.getDataSize() - 1;
	out->assign(data, data + dataSize);

	err->assign("");
	return true;
}

static bool WriteWholeFileFunction(std::string* err, const std::string& filepath, const std::vector<unsigned char>& contents, void*)
{
	FRM_ASSERT(false);
	return false;
}

bool MeshData::ReadGltf(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{
	tinygltf::FsCallbacks callbacks;
	callbacks.FileExists = &FileExistsFunction;
	callbacks.ExpandFilePath = &ExpandFilePathFunction;
	callbacks.ReadWholeFile = &ReadWholeFileFunction;
	callbacks.WriteWholeFile = &WriteWholeFileFunction;
	callbacks.user_data = &mesh_;
	tinygltf::TinyGLTF loader;
	loader.SetFsCallbacks(callbacks);

	std::string err;
	std::string warn;
	tinygltf::Model gltf;
	if (!loader.LoadASCIIFromString(&gltf, &err, &warn, _srcData, (unsigned int)_srcDataSize, ""))
	{
		FRM_LOG_ERR(err.c_str());
		return false;
	}

	auto GetMatrixd = [](const double* m) -> mat4
		{
			mat4 ret;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					ret[i][j] = (float)m[i * 4 + j];
				}
			}
			return ret;
		};

	auto GetMatrixf = [](const float* m) -> mat4
		{
			mat4 ret;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					ret[i][j] = m[i * 4 + j];
				}
			}
			return ret;
		};

	auto GetTransform = [GetMatrixd](const tinygltf::Node* node) -> mat4
	{
		if (node->matrix.empty())
		{
			vec3 translation = vec3(0.0f);
			if (!node->translation.empty())
			{
				FRM_ASSERT(node->translation.size() == 3);
				translation = vec3(node->translation[0], node->translation[1], node->translation[2]);
			}

			quat rotation = quat(0.0f, 0.0f, 0.0f, 1.0f);
			if (!node->rotation.empty())
			{
				FRM_ASSERT(node->rotation.size() == 4);
				rotation = quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
			}

			vec3 scale = vec3(1.0f);
			if (!node->scale.empty())
			{
				FRM_ASSERT(node->scale.size() == 3);
				scale = vec3(node->scale[0], node->scale[1], node->scale[2]);
			}

			return TransformationMatrix(translation, rotation, scale);
		}
		else
		{		
			FRM_ASSERT(node->matrix.size() == 16);
			return GetMatrixd(node->matrix.data());
		}
	};

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
						const mat4 transform = GetMatrixf((const float*)buffer);
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
						tinygltf::Accessor normalsAccessor = gltf.accessors[meshPrimitive.attributes["NORMAL"]];
						FRM_ASSERT(normalsAccessor.count == positionsAccessor.count);
						FRM_ASSERT(normalsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
						FRM_ASSERT(normalsAccessor.type == TINYGLTF_TYPE_VEC3);
	
						const auto& bufferView = gltf.bufferViews[normalsAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + normalsAccessor.byteOffset;
						const size_t stride = normalsAccessor.ByteStride(bufferView);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex, buffer += stride)
						{
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vertex.m_normal = TransformDirection(transform, Normalize(*((vec3*)buffer)));
						}
					}
					else
					{
						generateNormals = generateTangents = true;
					}

					if (meshPrimitive.attributes.find("TANGENT") != meshPrimitive.attributes.end())
					{
						tinygltf::Accessor tangentsAccessor = gltf.accessors[meshPrimitive.attributes["TANGENT"]];
						FRM_ASSERT(tangentsAccessor.count == positionsAccessor.count);
						FRM_ASSERT(tangentsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
						FRM_ASSERT(tangentsAccessor.type == TINYGLTF_TYPE_VEC4);
				
						const auto& bufferView = gltf.bufferViews[tangentsAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + tangentsAccessor.byteOffset;
						const size_t stride = tangentsAccessor.ByteStride(bufferView);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex, buffer += stride)
						{
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vertex.m_tangent = *((vec4*)buffer);
							vertex.m_tangent = vec4(TransformDirection(transform, Normalize(vertex.m_tangent.xyz())), vertex.m_tangent.w);
						}
					}
					else
					{
						generateTangents = true;
					}

					if (meshPrimitive.attributes.find("TEXCOORD_0") != meshPrimitive.attributes.end())
					{
						tinygltf::Accessor texcoordsAccessor = gltf.accessors[meshPrimitive.attributes["TEXCOORD_0"]];
						FRM_ASSERT(texcoordsAccessor.count == positionsAccessor.count);
						FRM_ASSERT(texcoordsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
						FRM_ASSERT(texcoordsAccessor.type == TINYGLTF_TYPE_VEC2);

						const auto& bufferView = gltf.bufferViews[texcoordsAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + texcoordsAccessor.byteOffset;
						const size_t stride = texcoordsAccessor.ByteStride(bufferView);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex, buffer += stride)
						{
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vertex.m_texcoord = *((vec2*)buffer);
						}
					}

					if (meshPrimitive.attributes.find("JOINTS_0") != meshPrimitive.attributes.end())
					{
						hasBoneIndices = true;

						tinygltf::Accessor boneIndicesAccessor = gltf.accessors[meshPrimitive.attributes["JOINTS_0"]];
						FRM_ASSERT(boneIndicesAccessor.count == positionsAccessor.count);
						FRM_ASSERT(boneIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
						FRM_ASSERT(boneIndicesAccessor.type == TINYGLTF_TYPE_VEC4);

						const auto& bufferView = gltf.bufferViews[boneIndicesAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + boneIndicesAccessor.byteOffset;
						const size_t stride = boneIndicesAccessor.ByteStride(bufferView);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex, buffer += stride)
						{
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							for (int i = 0; i < 4; ++i)
							{
								const uint16 boneIndex = ((uint16*)buffer)[i];
								vertex.m_boneIndices[i] = (uint32)boneIndexMap[boneIndex];
							}
						}
					}					

					if (meshPrimitive.attributes.find("WEIGHTS_0") != meshPrimitive.attributes.end())
					{
						hasBoneWeights = true;

						tinygltf::Accessor boneWeightsAccessor = gltf.accessors[meshPrimitive.attributes["WEIGHTS_0"]];
						FRM_ASSERT(boneWeightsAccessor.count == positionsAccessor.count);
						FRM_ASSERT(boneWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
						FRM_ASSERT(boneWeightsAccessor.type == TINYGLTF_TYPE_VEC4);

						const auto& bufferView = gltf.bufferViews[boneWeightsAccessor.bufferView];
						const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + boneWeightsAccessor.byteOffset;
						const size_t stride = boneWeightsAccessor.ByteStride(bufferView);
						for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex, buffer += stride)
						{
							MeshBuilder::Vertex& vertex = meshBuilder.getVertex(vertexOffset + vertexIndex);
							vertex.m_boneWeights = *((vec4*)buffer);
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

	/*if (!meshDesc.findVertexAttr(VertexAttr::Semantic_BoneWeights) && hasBoneWeights)
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
	}*/

	MeshData retMesh(meshDesc, finalMeshBuilder);
	swap(mesh_, retMesh);

	mesh_.setBindPose(bindPose.data(), bindPose.size());

	return true;
}
