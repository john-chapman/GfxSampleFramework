#include "SkeletonAnimation.h"

#include <frm/core/hash.h>
#include <frm/core/interpolation.h>
#include <frm/core/log.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Serializer.h>
#include <frm/core/Time.h>

#include <im3d/im3d.h>

using namespace frm;

/******************************************************************************

                                 Skeleton

******************************************************************************/

// PUBLIC

Skeleton::Skeleton()
{
	// Bone is cast to a float* during sampling, check alignments/offsets:
	FRM_STATIC_ASSERT(alignof(Bone) >= alignof(float));
	FRM_STATIC_ASSERT(offsetof(Bone, translation) == 0);
	FRM_STATIC_ASSERT(offsetof(Bone, rotation) == 3 * sizeof(float));
	FRM_STATIC_ASSERT(offsetof(Bone, scale) == 7 * sizeof(float));
}

bool Skeleton::serialize(Serializer& _serializer_)
{
	bool ret = true;

	uint poseSize = m_pose.size();
	if (_serializer_.beginArray(poseSize, "m_pose"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			m_pose.resize(poseSize);
		}

		for (mat4& m : m_pose)
		{
			ret &= Serialize(_serializer_, m);
		}

		_serializer_.endArray();
	}
	else
	{
		ret = false;
	}

	uint boneCount = m_bones.size();
	if (_serializer_.beginArray(boneCount, "m_bones"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			m_bones.resize(boneCount);
		}

		for (Bone& bone : m_bones)
		{
			_serializer_.beginObject();
				Serialize(_serializer_, bone.translation, "translation");
				Serialize(_serializer_, bone.rotation,    "rotation");
				Serialize(_serializer_, bone.scale,       "scale");
				ret &= Serialize(_serializer_, bone.parentIndex, "parentIndex");
			_serializer_.endObject();
		}

		_serializer_.endArray();
	}
	else
	{
		ret = false;
	}
	
	uint boneIdCount = m_boneIds.size();
	if (_serializer_.beginArray(boneIdCount, "m_boneIds"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			m_boneIds.resize(boneIdCount);
		}

		for (uint32& boneId : m_boneIds)
		{
			ret &= Serialize(_serializer_, boneId);
		}

		_serializer_.endArray();
	}
	else
	{
		ret = false;
	}

	uint boneNameCount = m_boneNames.size();
	if (_serializer_.beginArray(boneNameCount, "m_boneNames"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			m_boneNames.resize(boneNameCount);
		}

		for (BoneName& boneName : m_boneNames)
		{
			ret &= Serialize(_serializer_, boneName);
		}

		_serializer_.endArray();
	}
	else
	{
		ret = false;
	}

	return ret;
}

int Skeleton::addBone(const char* _name, int _parentIndex)
{
	FRM_ASSERT(_parentIndex < getBoneCount());
	int ret = (int)m_bones.size();
	m_bones.push_back(Bone());
	m_boneIds.push_back(HashString<BoneId>(_name));
	m_boneNames.push_back(_name);
	m_pose.resize(m_bones.size());
	return ret;
}

const mat4* Skeleton::resolve()
{
	FRM_ASSERT(m_pose.size() == m_bones.size());

	for (int i = 0, n = getBoneCount(); i < n; ++i)
	{
		const Bone& bone = m_bones[i];

		mat4 m = TransformationMatrix(bone.translation, bone.rotation, bone.scale);

		if (bone.parentIndex >= 0)
		{
			FRM_ASSERT(bone.parentIndex <= i); // parent must come before children

			// \todo Cheaper to apply the parent position/orientation/scale separately then build the matrix?
			m = m_pose[bone.parentIndex] * m;
		}

		m_pose[i] = m;
	}

	return m_pose.data();
}

void Skeleton::draw() const
{
	Im3d::PushDrawState();
	 
	Im3d::SetColor(Im3d::Color_White);
	Im3d::SetAlpha(0.2f);
	Im3d::BeginLines();
		for (int i = 0, n = (int)m_bones.size(); i < n; ++i)
		{
			const Bone& bone = m_bones[i];
			if (bone.parentIndex >= 0)
			{
				Im3d::Vertex(GetTranslation(m_pose[i]), 2.0f);
				Im3d::Vertex(GetTranslation(m_pose[bone.parentIndex]), 12.0f);
			}
		}
	Im3d::End();

	Im3d::SetAlpha(1.0f);
	for (auto& m : m_pose)
	{
		Im3d::PushMatrix();
		Im3d::MulMatrix(m);
			float s = Im3d::GetContext().pixelsToWorldSize(GetTranslation(m), 16.0f);
			Im3d::Scale(s, s, s);
			Im3d::DrawXyzAxes();
			Im3d::DrawPoint(vec3(0.0f), 8.0f, Im3d::Color_White);
		Im3d::PopMatrix();
	}

	Im3d::PopDrawState();
}


/******************************************************************************

                           SkeletonAnimationTrack

******************************************************************************/

// PUBLIC

void SkeletonAnimationTrack::sample(float _t, float* out_, int* _hint_)
{
	int i;
	if (_hint_ == nullptr)
	{ 
		// No hint, use binary search.
		i = findFrame(_t);
	}
	else
	{ 
		// Hint, use linear search.
		i = *_hint_;
		if_unlikely (_t < m_frames[i])
		{
			i = findFrame(_t);
		}
		else
		{
			while (_t > m_frames[i + 1])
			{
				i = (i + 1) % (int)m_frames.size();
			}
			*_hint_ = i;
		}
	}
	
	FRM_ASSERT(i < m_data.size());
	FRM_ASSERT(i * m_boneDataSize < m_data.size());
	FRM_ASSERT((i + 1) * m_boneDataSize < m_data.size());
	float t = (_t - m_frames[i]) / (m_frames[i + 1] - m_frames[i]);
	const float* a = &m_data[i * m_boneDataSize];
	const float* b = &m_data[(i + 1) * m_boneDataSize];
#if 0
	// Straight lerp.
	for (int j = 0; j < m_boneDataSize; ++j)
	{
		out_[j] = lerp(a[j], b[j], t);
	}
#else
	if (m_boneDataSize == 3)
	{
		*((vec3*)out_) = lerp(*((vec3*)a), *((vec3*)b), t);
	}
	else if (m_boneDataSize == 4) // Assume 4 float data is a quaternion, do slerp.
	{
		//*((quat*)out_) = slerp(*((quat*)a), *((quat*)b), t);
		*((quat*)out_) = linalg::qslerp(*((quat*)a), *((quat*)b), t);
	}
	else
	{
		for (int j = 0; j < m_boneDataSize; ++j)
		{
			out_[j] = lerp(a[j], b[j], t);
		}
	}
#endif

}

void SkeletonAnimationTrack::addFrames(int _count, const float* _normalizedTimes, const float* _data)
{
	FRM_ASSERT(m_frames.empty() || m_frames.back() < *_normalizedTimes);
	for (int i = 0; i < _count; ++i)
	{
		FRM_ASSERT(*_normalizedTimes >= 0.0f && *_normalizedTimes <= 1.0f); // times must be normalized by the track duration
		
		m_frames.push_back(*(_normalizedTimes++));
		for (int j = 0; j < m_boneDataSize; ++j)
		{
			m_data.push_back(*(_data++));
		}
	}
}

// PRIVATE

SkeletonAnimationTrack::SkeletonAnimationTrack(int _boneIndex, int _boneDataOffset, int _boneDataSize, int _frameCount, float* _normalizedTimes, float* _data)
	: m_boneIndex(_boneIndex)
	, m_boneDataOffset(_boneDataOffset)
	, m_boneDataSize(_boneDataSize)
{
	if (_frameCount > 0 && _normalizedTimes != nullptr)
	{
		m_frames.assign(_normalizedTimes, _normalizedTimes + _frameCount);
	}
	
	if (_frameCount > 0 && _data)
	{
		m_data.assign(_data, _data + _frameCount * _boneDataSize);
	}
}


int SkeletonAnimationTrack::findFrame(float _t)
{
	int lo = 0, hi = (int)m_frames.size() - 1;
	while (hi - lo > 1)
	{
		int mid = (hi + lo) / 2;		
		if (_t > m_frames[mid])
		{
			lo = mid;
		}
		else
		{
			hi = mid;
		}
	}
	return _t > m_frames[hi] ? hi : lo;
}

/******************************************************************************

                              SkeletonAnimation

******************************************************************************/

// PUBLIC

SkeletonAnimation* SkeletonAnimation::Create(const char* _path)
{
	Id id = GetHashId(_path);
	SkeletonAnimation* ret = Find(id);
	if (!ret)
	{
		ret = new SkeletonAnimation(id, _path);
		ret->m_path.set(_path);
	}
	
	Use(ret);
	if (!CheckResource(ret))
	{
		FRM_LOG_ERR("Error loading SkeletonAnimation '%s'", _path);
	}

	return ret;
}
void SkeletonAnimation::Destroy(SkeletonAnimation*& _inst_)
{
	delete _inst_;
}

bool SkeletonAnimation::reload()
{
	if (m_path.isEmpty())
	{
		return true;
	}

	FRM_AUTOTIMER("SkeletonAnimation::load(%s)",  m_path.c_str());

	File f;
	if (!FileSystem::Read(f, (const char*)m_path))
	{
		return false;
	}

	if (FileSystem::CompareExtension("gltf", m_path.c_str()))
	{
		return ReadGltf(*this, f.getData(), f.getDataSize());
	}
	else if (FileSystem::CompareExtension("md5anim", m_path.c_str()))
	{
		return ReadMd5(*this, f.getData(), f.getDataSize());
	}
	else
	{
		FRM_ASSERT(false); // unsupported format
	}

	return false;
}


void SkeletonAnimation::sample(float _t, Skeleton& _out_, int _hints_[])
{
	for (auto& track : m_tracks)
	{
		float* out = (float*)&_out_.getBone(track.getBoneIndex());
		out += track.getBoneDataOffset();
		track.sample(_t, out, _hints_);
		if (_hints_)
		{
			++_hints_;
		}
	}
}

SkeletonAnimationTrack* SkeletonAnimation::addTranslationTrack(int _boneIndex, int _frameCount, float* _normalizedTimes, float* _data)
{
	const int offset = offsetof(Skeleton::Bone, translation) / sizeof(float);
	FRM_ASSERT(findTrack(_boneIndex, offset, 3) == nullptr); // track already exists
	m_tracks.push_back(SkeletonAnimationTrack(_boneIndex, offset, 3, _frameCount, _normalizedTimes, _data));
	return &m_tracks.back();
}
SkeletonAnimationTrack* SkeletonAnimation::addRotationTrack(int _boneIndex, int _frameCount, float* _normalizedTimes, float* _data)
{
	const int offset = offsetof(Skeleton::Bone, rotation) / sizeof(float);
	FRM_ASSERT(findTrack(_boneIndex, offset, 4) == nullptr); // track already exists
	m_tracks.push_back(SkeletonAnimationTrack(_boneIndex, offset, 4, _frameCount, _normalizedTimes, _data));
	return &m_tracks.back();
}
SkeletonAnimationTrack* SkeletonAnimation::addScaleTrack(int _boneIndex, int _frameCount, float* _normalizedTimes, float* _data)
{
	const int offset = offsetof(Skeleton::Bone, scale) / sizeof(float);
	FRM_ASSERT(findTrack(_boneIndex, offset, 3) == nullptr); // track already exists
	m_tracks.push_back(SkeletonAnimationTrack(_boneIndex, offset, 3, _frameCount, _normalizedTimes, _data));
	return &m_tracks.back();
}

// PROTECTED

SkeletonAnimation::SkeletonAnimation(uint64 _id, const char* _name)
	: Resource(_id, _name)
{
}

SkeletonAnimation::~SkeletonAnimation()
{
}

// PRIVATE

SkeletonAnimationTrack* SkeletonAnimation::findTrack(int _boneIndex, int _boneDataOffset, int _boneDataSize)
{
	for (auto& track : m_tracks)
	{
		if (track.m_boneIndex == _boneIndex && track.m_boneDataOffset == _boneDataOffset && track.m_boneDataSize == _boneDataSize)
		{
			return &track;
		}
	}
	return nullptr;
}
