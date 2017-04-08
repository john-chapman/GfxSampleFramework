#include <frm/SkeletonAnimation.h>

#include <apt/TextParser.h>

#include <extern/md5mesh.h>

#include <EASTL/sort.h>
#include <EASTL/vector.h>

using namespace frm;
using namespace apt;

bool SkeletonAnimation::ReadMd5(SkeletonAnimation& anim_, const char* _srcData, uint _srcDataSize)
{	
	TextParser tp(_srcData);

	long int numJoints = -1;
	long int numFrames = -1;
	long int frameRate = -1;
	long int numAnimatedComponents = -1;
	md5_call(md5::ParseAnimHeader(tp, &numJoints, &numFrames, &frameRate, &numAnimatedComponents));

	eastl::vector<md5::AnimJoint> joints;
	joints.resize(numJoints);
	eastl::vector<md5::Bounds> bounds;
	bounds.resize(numFrames);
	eastl::vector<float> frames;
	frames.resize(numFrames * numAnimatedComponents);
	
	long int frameCount = 0;
	while (!tp.isNull()) {
		if (tp.compareNext("hierarchy")) {
			md5_call(md5::ParseAnimJointList(tp, numJoints, joints.data()));
			continue;
		}
		if (tp.compareNext("bounds")) {
			md5_call(md5::ParseBoundsList(tp, numFrames, bounds.data()));
			continue;
		}
		if (tp.compareNext("baseframe")) {
			md5_call(md5::ParseBaseFrame(tp, numJoints, joints.data()));
			continue;
		}
		if (tp.compareNext("frame")) {
			long int frameIndex;
			md5_call(md5::ParseInt(tp, &frameIndex));
			if (frameIndex > frameCount) {
				md5_err("frames out of order (expected frame %d, found frame %d)", frameCount, frameIndex);
			}
			if (frameCount > numFrames) {
				md5_err("too many frames (expected %d)", numFrames);
			}
			md5_call(md5::ParseFrame(tp, numAnimatedComponents, frames.data() + numAnimatedComponents * frameCount));
			++frameCount;
		}
	}
	if (frameCount < numFrames) {
		md5_err("too few frames (expected %d)", numFrames);
	}

	float duration = (float)numFrames / (float)frameRate;

 // base frame skeleton 
	Skeleton baseFrame;
	for (auto& src : joints) {
		int i = baseFrame.addBone(src.m_name, src.m_parentIndex);
		Skeleton::Bone& bone = baseFrame.getBone(i);
		bone.m_scale = vec3(1.0f);
		bone.m_position = src.m_position;
		bone.m_orientation = src.m_orientation;
		bone.m_parentIndex = src.m_parentIndex == i ? -1 : src.m_parentIndex; // if the bone's parent is itself, make it a root
	}
	anim_.m_baseFrame = baseFrame;
	anim_.m_baseFrame.resolve();

	eastl::vector<float> normalizedTimes;
	for (int i = 0; i < numFrames; ++i) {
		normalizedTimes.push_back((float)i / (float)(numFrames - 1));
	}
	anim_.m_tracks.clear();
 
 // extract position/orientation tracks
	for (int i = 0; i < baseFrame.getBoneCount(); ++i) {
		Skeleton::Bone& bone = baseFrame.getBone(i);
		md5::AnimJoint& joint = joints[i];

		eastl::vector<vec3> positions;
		positions.reserve(numFrames);
		eastl::vector<quat> orientations;
		orientations.reserve(numFrames);

		for (int j = 0; j < numFrames; ++j) {
			int cmp = 0; // increment per used component

			vec3 pos = bone.m_position; // base position
			if (joint.m_flags & md5::Position_X) {
				pos.x = frames[j * numAnimatedComponents + joint.m_startIndex + cmp++];
			}
			if (joint.m_flags & md5::Position_Y) {
				pos.y = frames[j * numAnimatedComponents + joint.m_startIndex + cmp++];
			}
			if (joint.m_flags & md5::Position_Z) {
				pos.z = frames[j * numAnimatedComponents + joint.m_startIndex + cmp++];
			}
			positions.push_back(pos);

			quat ori = bone.m_orientation; // base orientation
			ori.w = 0.0f; // reconstruct w below

			if (joint.m_flags & md5::Orientation_X) {
				ori.x = frames[j * numAnimatedComponents + joint.m_startIndex + cmp++];
			}
			if (joint.m_flags & md5::Orientation_Y) {
				ori.y = frames[j * numAnimatedComponents + joint.m_startIndex + cmp++];
			}
			if (joint.m_flags & md5::Orientation_Z) {
				ori.z = frames[j * numAnimatedComponents + joint.m_startIndex + cmp++];
			}
			float t = 1.0f - length2(ori);
			ori.w = t < 0.0f ? 0.0f : -sqrtf(t);
			orientations.push_back(ori);
		}

		if (joint.m_flags & md5::Position_Mask) {
			anim_.addPositionTrack(i, numFrames, normalizedTimes.data(), (float*)positions.data());
		}
		if (joint.m_flags & md5::Orientation_Mask) {
			anim_.addOrientationTrack(i, numFrames, normalizedTimes.data(), (float*)orientations.data());
		}
	}

	return true;
}