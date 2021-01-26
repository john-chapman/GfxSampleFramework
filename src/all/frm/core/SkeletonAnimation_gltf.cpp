/* \todo
	- Load + store 'base' skeleton with the mesh. This provides an authoritative skeleton for procedural animation, physics setup, etc.
	- Map animation tracks to bone IDs rather than bone indices. This allows animations to be shared between skeletons (e.g. for LODing).
		- E.g. animation track references bone HEAD, anim controller then maps HEAD -> bone index on the target skeleton.
*/

#include "SkeletonAnimation.h"

#include <frm/core/log.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Time.h>

#include <gltf.h>
#include <string>

namespace frm {

bool SkeletonAnimation::ReadGltf(SkeletonAnimation& anim_, const char* _srcData, uint _srcDataSize)
{
	tinygltf::Model gltf;
	const PathStr rootPath = FileSystem::GetPath(anim_.getPath());
	if (!tinygltf::Load(_srcData, _srcDataSize, rootPath.c_str(), gltf))
	{
		return false;
	}

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
							bone.m_translation = vec3(joint.translation[0], joint.translation[1], joint.translation[2]);
						}

						if (!joint.scale.empty())
						{
							bone.m_scale = vec3(joint.scale[0], joint.scale[1], joint.scale[2]);
						}

						if (!joint.rotation.empty())
						{
							bone.m_rotation = quat(joint.rotation[0], joint.rotation[1], joint.rotation[2], joint.rotation[3]);
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

			tinygltf::AutoAccessor frameTimesAccessor(gltf.accessors[animSampler.input], gltf);
			eastl::vector<float> frameTimes(frameTimesAccessor.getCount());
			frameTimesAccessor.copyBytes(frameTimes.data());

			float timeMin = FLT_MAX;
			float timeMax = -FLT_MAX;
			for (float time : frameTimes)
			{
				timeMin = Min(time, timeMin);
				timeMax = Max(time, timeMax);
			}
			for (float& time : frameTimes)
			{
				time = (time - timeMin) / (timeMax - timeMin);
			}


			tinygltf::AutoAccessor frameDataAccessor(gltf.accessors[animSampler.output], gltf);
			eastl::vector<float> frameData(frameDataAccessor.getSizeBytes() / sizeof(float));
			frameDataAccessor.copyBytes(frameData.data());

			const int frameCount = (int)frameTimes.size();
			if (channel.target_path == "translation")
			{
				anim_.addTranslationTrack(targetBoneIndex, frameCount, frameTimes.data(), frameData.data());
			}
			else if (channel.target_path == "rotation")
			{
				anim_.addRotationTrack(targetBoneIndex, frameCount, frameTimes.data(), frameData.data());
			}
			else if (channel.target_path == "scale")
			{
				anim_.addScaleTrack(targetBoneIndex, frameCount, frameTimes.data(), frameData.data());
			}
		}
	}

	return true;
}

} // namespace frm