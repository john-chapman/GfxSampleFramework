#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/Resource.h>
#include <frm/core/String.h>

#include <EASTL/vector.h>

namespace frm {

///////////////////////////////////////////////////////////////////////////////
// Skeleton
// Hierarchy of bones, stored as local space position/orientation/scale. 
///////////////////////////////////////////////////////////////////////////////
class Skeleton
{
public:

	using BoneName = String<16>;
	using BoneId   = uint32;
	
	struct Bone
	{
		vec3 m_translation  = vec3(0.0f);
		quat m_rotation     = quat(0.0f, 0.0f, 0.0f, 1.0f);
		vec3 m_scale        = vec3(1.0f);
		int  m_parentIndex  = -1; // -1 = root bone
	};

	             Skeleton();

	// Return the index of the new bone.
	int          addBone(const char* _name, int _parentIndex = -1);

	// Resolve bone hierarchy into final pose. 
	const mat4*  resolve();

	void         draw() const;

	const mat4*  getPose() const                         { return m_pose.data(); }
	      mat4*  getPose()                               { return m_pose.data(); }
	      int    getBoneCount() const                    { return (int)m_bones.size();          }
	      int    getBoneIndex(const Bone& _bone) const   { return (int)(&_bone - &m_bones[0]);  }
	const Bone&  getBone(int _index) const               { return m_bones[_index];     }
	      Bone&  getBone(int _index)                     { return m_bones[_index];     }
	      BoneId getBoneId(int _index) const             { return m_boneIds[_index];   }
	const char*  getBoneName(int _index) const           { return (const char*)m_boneNames[_index]; }

private:

	eastl::vector<mat4>     m_pose;
	eastl::vector<Bone>     m_bones;
	eastl::vector<BoneId>   m_boneIds;
	eastl::vector<BoneName> m_boneNames;
};

///////////////////////////////////////////////////////////////////////////////
// SkeletonAnimationTrack
// Ordered list of frame data and normalized frame times.
///////////////////////////////////////////////////////////////////////////////
class SkeletonAnimationTrack
{
	friend class SkeletonAnimation;
public:

	// Evaluate the track at _t (in [0,1]), writing m_dataCount floats to out_. 
	// _hint_ is useful in the common case where evaluate() is called repeatedly 
	// with a monotonically increasing _t, it avoids performing a binary search 
	// on the track data.
	void sample(float _t, float* out_, int* _hint_ = nullptr);

	void addFrames(int _count, const float* _normalizedTimes, const float* _data);
	
	int  getBoneIndex() const       { return m_boneIndex; }
	int  getBoneDataOffset() const  { return m_boneDataOffset; }
	int  getBoneDataSize() const    { return m_boneDataSize; }
	
private:

	int m_boneIndex;
	int m_boneDataOffset;  // result offset in Skeleton::Bone
	int m_boneDataSize;    // number of floats per frame

	eastl::vector<float> m_frames; // track position in [0,1] associated with each keyframe
	eastl::vector<float> m_data;   // m_count floats per keyframe

	SkeletonAnimationTrack(int _boneIndex, int _boneDataOffset, int _boneDataSize, int _frameCount, float* _normalizedTimes, float* _data);

	// Find the index of the first frame in the segment containing _t.
	int findFrame(float _t);
};

///////////////////////////////////////////////////////////////////////////////
// SkeletonAnimation
// A single animation clip comprised of some number of tracks.
///////////////////////////////////////////////////////////////////////////////
class SkeletonAnimation: public Resource<SkeletonAnimation>
{
public:

	static SkeletonAnimation* Create(const char* _path);
	static void Destroy(SkeletonAnimation*& _inst_);

	bool load()   { return reload(); }
	bool reload();

	void sample(float _t, Skeleton& out_, int _hints_[] = nullptr);

	// \note add* functions invalidate ptrs previously returned.
	SkeletonAnimationTrack* addTranslationTrack(int _boneIndex, int _frameCount = 0, float* _normalizedTimes = nullptr, float* _data = nullptr);
	SkeletonAnimationTrack* addRotationTrack(int _boneIndex, int _frameCount = 0, float* _normalizedTimes = nullptr, float* _data = nullptr);
	SkeletonAnimationTrack* addScaleTrack(int _boneIndex, int _frameCount = 0, float* _normalizedTimes = nullptr, float* _data = nullptr);

	int                     getTrackCount() const { return (int)m_tracks.size(); }
	const Skeleton&         getBaseFrame() const  { return m_baseFrame; }
	const char*             getPath() const       { return m_path.c_str(); }

protected:

	SkeletonAnimation(uint64 _id, const char* _name);
	~SkeletonAnimation();

private:

	frm::String<32> m_path; // empty if not from a file
	eastl::vector<SkeletonAnimationTrack> m_tracks;
	Skeleton m_baseFrame;

	SkeletonAnimationTrack* findTrack(int _boneIndex, int _boneDataOffset, int _boneDataSize);

	static bool ReadMd5(SkeletonAnimation& anim_, const char* _srcData, uint _srcDataSize);
	static bool ReadGltf(SkeletonAnimation& anim_, const char* _srcData, uint _srcDataSize);

}; // class SkeletonAnimation

} // namespace frm

