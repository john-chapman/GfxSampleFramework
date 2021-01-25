/* \todo
	- Share gltf code with MeshData_gltf + helper class for iterating over an accessor.
	- Load + store 'base' skeleton with the mesh. This provides an authoritative skeleton for procedural animation, physics setup, etc.
	- Map animation tracks to bone IDs rather than bone indices. This allows animations to be shared between skeletons (e.g. for LODing).
		- E.g. animation track references bone HEAD, anim controller then maps HEAD -> bone index on the target skeleton.
	- Optimize for non-sparse buffer views (can just memcpy or use DataConvert).
*/

#include "SkeletonAnimation.h"

#include <frm/core/log.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Time.h>

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

static std::string ExpandFilePathFunction(const std::string& path, void* anim)
{
	// assume any internal URIs are relative to the source .gltf file
	PathStr animPath = FileSystem::GetPath(((SkeletonAnimation*)anim)->getPath());
	animPath.appendf("/%s", path.c_str());
	return animPath.c_str();
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

bool SkeletonAnimation::ReadGltf(SkeletonAnimation& anim_, const char* _srcData, uint _srcDataSize)
{
	tinygltf::FsCallbacks callbacks;
	callbacks.FileExists = &FileExistsFunction;
	callbacks.ExpandFilePath = &ExpandFilePathFunction;
	callbacks.ReadWholeFile = &ReadWholeFileFunction;
	callbacks.WriteWholeFile = &WriteWholeFileFunction;
	callbacks.user_data = &anim_;
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

	Skeleton baseFrame;
	//eastl::vector<SkeletonAnimationTrack> animTracks;
	eastl::vector<int> boneIndexMap(gltf.nodes.size(), -1); // Map node indices -> bone indices.

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

				if (node.skin != -1)
				{
					const auto& skin = gltf.skins[node.skin];

					for (int jointIndex : skin.joints)
					{
						const auto& joint = gltf.nodes[jointIndex];
						int boneIndex = baseFrame.addBone(joint.name.c_str(), -1);
						Skeleton::Bone& bone = baseFrame.getBone(boneIndex);
						FRM_ASSERT(boneIndexMap[jointIndex] == -1);
						boneIndexMap[jointIndex] = boneIndex;

						if (!joint.translation.empty())
						{
							bone.m_position = vec3(joint.translation[0], joint.translation[1], joint.translation[2]);
						}

						if (!joint.scale.empty())
						{
							bone.m_scale = vec3(joint.scale[0], joint.scale[1], joint.scale[2]);
						}

						if (!joint.rotation.empty())
						{
							bone.m_orientation = quat(joint.rotation[0], joint.rotation[1], joint.rotation[2], joint.rotation[3]);
						}
					}

					for (int jointIndex : skin.joints)
					{
						const auto& joint = gltf.nodes[jointIndex];
						int parentIndex = boneIndexMap[jointIndex];
						for (int childIndex : joint.children)
						{
							int boneIndex = boneIndexMap[childIndex];
							baseFrame.getBone(boneIndex).m_parentIndex = parentIndex;
						}
					}
				}
			}
		}
	}
// \todo Don't modify anim_ until loading is complete, as per mesh loaders.
	anim_.m_baseFrame = baseFrame;
	anim_.m_baseFrame.resolve();

	anim_.m_tracks.clear();
	for (auto& animation : gltf.animations)
	{
		for (auto& channel : animation.channels)
		{
			const int targetBoneIndex = boneIndexMap[channel.target_node];
			if (targetBoneIndex == -1)
			{
				continue;
			}

			const tinygltf::AnimationSampler animSampler = animation.samplers[channel.sampler];

			eastl::vector<float> frameTimes;
			{
				const tinygltf::Accessor inAccessor = gltf.accessors[animSampler.input];
				FRM_ASSERT(inAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				FRM_ASSERT(inAccessor.type == TINYGLTF_TYPE_SCALAR);

				const auto& bufferView = gltf.bufferViews[inAccessor.bufferView];
				const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + inAccessor.byteOffset;
				const size_t stride = inAccessor.ByteStride(bufferView);

				float timeMin = FLT_MAX;
				float timeMax = -FLT_MAX;
				frameTimes.reserve(inAccessor.count);
				for (size_t frameIndex = 0; frameIndex < inAccessor.count; ++frameIndex, buffer += stride)
				{
					const float time = *((float*)buffer);
					timeMin = Min(time, timeMin);
					timeMax = Max(time, timeMax);
					frameTimes.push_back(time);
				}

				for (float& time : frameTimes)
				{
					time = (time - timeMin) / (timeMax - timeMin);
				}
			}

			eastl::vector<float> frameData;
			{
				const tinygltf::Accessor outAccessor = gltf.accessors[animSampler.output];
				FRM_ASSERT(outAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				int count = 0;
				switch (outAccessor.type)
				{
					default:
					case TINYGLTF_TYPE_SCALAR:
						count = 1;
						break;
					case TINYGLTF_TYPE_VEC2:
						count = 2;
						break;
					case TINYGLTF_TYPE_VEC3:
						count = 3;
						break;
					case TINYGLTF_TYPE_VEC4:
						count = 4;
						break;
				};
				FRM_ASSERT(count != 0);

				// \todo Optimize for non-sparse buffer views (can just memcpy or use DataConvert).
				const auto& bufferView = gltf.bufferViews[outAccessor.bufferView];
				const unsigned char* buffer = gltf.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + outAccessor.byteOffset;
				const size_t stride = outAccessor.ByteStride(bufferView);
				for (size_t frameIndex = 0; frameIndex < outAccessor.count; ++frameIndex, buffer += stride)
				{
					for (int i = 0; i < count; ++i)
					{
						frameData.push_back(((float*)buffer)[i]);
					}
				}
			}

			const int frameCount = (int)frameTimes.size();
			if (channel.target_path == "translation")
			{
				anim_.addPositionTrack(targetBoneIndex, frameCount, frameTimes.data(), frameData.data());
			}
			else if (channel.target_path == "rotation")
			{
				anim_.addOrientationTrack(targetBoneIndex, frameCount, frameTimes.data(), frameData.data());
			}
			else if (channel.target_path == "scale")
			{
				anim_.addScaleTrack(targetBoneIndex, frameCount, frameTimes.data(), frameData.data());
			}
		}
	}

	return true;
}
