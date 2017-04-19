#include <frm/SkeletonAnimation.h>

#include <apt/hash.h>
#include <apt/log.h>
#include <apt/FileSystem.h>
#include <apt/Time.h>

#include <im3d/im3d.h>

using namespace frm;
using namespace apt;

/******************************************************************************

                                 Skeleton

******************************************************************************/

// PUBLIC

int Skeleton::addBone(const char* _name, int _parentIndex)
{
	APT_ASSERT(_parentIndex < getBoneCount());
	int ret = (int)m_bones.size();
	m_bones.push_back(Bone());
	m_boneIds.push_back(HashString<BoneId>(_name));
	m_boneNames.push_back(_name);
	m_pose.resize(m_bones.size());
	return ret;
}

const mat4* Skeleton::resolve()
{
	APT_ASSERT(m_pose.size() == m_bones.size());

	for (int i = 0, n = getBoneCount(); i < n; ++i) {
		const Bone& bone = m_bones[i];

	 // \todo optimize?
		mat4 m = 
			translate(mat4(1.0f), bone.m_position) * 
			mat4_cast(bone.m_orientation) * 
			scale(mat4(1.0f), bone.m_scale)
			;

		if (bone.m_parentIndex >= 0) {
			APT_ASSERT(bone.m_parentIndex <= i); // parent must come before children

		 // \todo cheaper to apply the parent position/orientation/scale separately then build the matrix?
			m = m_pose[bone.m_parentIndex] * m;
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
		for (int i = 0, n = (int)m_bones.size(); i < n; ++i) {
			const Bone& bone = m_bones[i];
			if (bone.m_parentIndex >= 0) {
				Im3d::Vertex(GetTranslation(m_pose[i]), 2.0f);
				Im3d::Vertex(GetTranslation(m_pose[bone.m_parentIndex]), 12.0f);
			}
		}
	Im3d::End();

	Im3d::SetAlpha(1.0f);
	for (auto& m : m_pose) {
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

Skeleton::Skeleton()
{
 // Bone is cast to a float* during sampling, check alignments/offsets:
	APT_STATIC_ASSERT(alignof(Bone) >= alignof(float));
	APT_STATIC_ASSERT(offsetof(Bone, m_position) == 0);
	APT_STATIC_ASSERT(offsetof(Bone, m_orientation) == 3 * sizeof(float));
	APT_STATIC_ASSERT(offsetof(Bone, m_scale) == 7 * sizeof(float));
}

/******************************************************************************

                           SkeletonAnimationTrack

******************************************************************************/

// PUBLIC

void SkeletonAnimationTrack::sample(float _t, float* out_, int* _hint_)
{
	int i;
	if (_hint_ == nullptr) { 
	 // no hint, use binary search
		i = findFrame(_t);
	} else { 
	 // hint, use linear search
		i = *_hint_;
		if_unlikely (_t < m_frames[i]) {
			i = findFrame(_t);
		} else {
			while (_t > m_frames[i + 1]) {
				i = (i + 1) % (int)m_frames.size();
			}
			*_hint_ = i;
		}
	}
	
	//APT_ASSERT(i < m_data.size());
	//APT_ASSERT(i * m_boneDataSize < m_data.size());
	//APT_ASSERT((i + 1) * m_boneDataSize < m_data.size());
	float t = (_t - m_frames[i]) / (m_frames[i + 1] - m_frames[i]);
	const float* a = &m_data[i * m_boneDataSize];
	const float* b = &m_data[(i + 1) * m_boneDataSize];
	//for (int j = 0; j < m_boneDataSize; ++j) {
	//	out_[j] = mix(a[j], b[j], t);
	//}
 // \hack where to renormalize quaternions?
if (m_boneDataSize == 3) {
	*((vec3*)out_) = mix(*((vec3*)a), *((vec3*)b), t);
} else if (m_boneDataSize == 4) {
	*((quat*)out_) = slerp(*((quat*)a), *((quat*)b), t);
} else {
	for (int j = 0; j < m_boneDataSize; ++j) {
		out_[j] = mix(a[j], b[j], t);
	}
}

}

void SkeletonAnimationTrack::addFrames(int _count, const float* _normalizedTimes, const float* _data)
{
	APT_ASSERT(m_frames.empty() || m_frames.back() < *_normalizedTimes);
	for (int i = 0; i < _count; ++i) {
		APT_ASSERT(*_normalizedTimes >= 0.0f && *_normalizedTimes <= 1.0f); // times must be normalized by the track duration
		
		m_frames.push_back(*(_normalizedTimes++));
		for (int j = 0; j < m_boneDataSize; ++j) {
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
	if (_frameCount > 0 && _normalizedTimes != nullptr) {
		m_frames.assign(_normalizedTimes, _normalizedTimes + _frameCount);
	}
	if (_frameCount > 0 && _data) {
		m_data.assign(_data, _data + _frameCount * _boneDataSize);
	}
}


int SkeletonAnimationTrack::findFrame(float _t)
{
	int lo = 0, hi = (int)m_frames.size() - 1;
	while (hi - lo > 1) {
		int mid = (hi + lo) / 2;		
		if (_t > m_frames[mid]) {
			lo = mid;
		} else {
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
	if (!ret) {
		ret = new SkeletonAnimation(id, _path);
		ret->m_path.set(_path);
	}
	Use(ret);
	if (ret->getState() != State_Loaded) {
		APT_LOG_ERR("Error loading SkeletonAnimation '%s'", _path);
	}
	return ret;
}
void SkeletonAnimation::Destroy(SkeletonAnimation*& _inst_)
{
	delete _inst_;
}

bool SkeletonAnimation::reload()
{
	if (m_path.isEmpty()) {
		return true;
	}

	APT_AUTOTIMER("SkeletonAnimation::load(%s)", (const char*)m_path);

	File f;
	if (!FileSystem::Read(f, m_path)) {
		return false;
	}

	if (FileSystem::CompareExtension("md5anim", m_path)) {
		return ReadMd5(*this, f.getData(), f.getDataSize());
	} else {
		APT_ASSERT(false); // unsupported format
	}
	return false;
}


void SkeletonAnimation::sample(float _t, Skeleton& _out_, int _hints_[])
{
	for (auto& track : m_tracks) {
		float* out = (float*)&_out_.getBone(track.getBoneIndex());
		out += track.getBoneDataOffset();
		track.sample(_t, out, _hints_);
		if (_hints_) {
			++_hints_;
		}
	}
}

SkeletonAnimationTrack* SkeletonAnimation::addPositionTrack(int _boneIndex, int _frameCount, float* _normalizedTimes, float* _data)
{
	int offset = offsetof(Skeleton::Bone, m_position) / sizeof(float);
	APT_ASSERT(findTrack(_boneIndex, offset, 3) == nullptr); // track already exists
	m_tracks.push_back(SkeletonAnimationTrack(_boneIndex, offset, 3, _frameCount, _normalizedTimes, _data));
	return &m_tracks.back();
}
SkeletonAnimationTrack* SkeletonAnimation::addOrientationTrack(int _boneIndex, int _frameCount, float* _normalizedTimes, float* _data)
{
	int offset = offsetof(Skeleton::Bone, m_orientation) / sizeof(float);
	APT_ASSERT(findTrack(_boneIndex, offset, 4) == nullptr); // track already exists
	m_tracks.push_back(SkeletonAnimationTrack(_boneIndex, offset, 4, _frameCount, _normalizedTimes, _data));
	return &m_tracks.back();
}
SkeletonAnimationTrack* SkeletonAnimation::addScaleTrack(int _boneIndex, int _frameCount, float* _normalizedTimes, float* _data)
{
	int offset = offsetof(Skeleton::Bone, m_scale) / sizeof(float);
	APT_ASSERT(findTrack(_boneIndex, offset, 3) == nullptr); // track already exists
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
	for (auto& track : m_tracks) {
		if (track.m_boneIndex == _boneIndex && track.m_boneDataOffset == _boneDataOffset && track.m_boneDataSize == _boneDataSize) {
			return &track;
		}
	}
	return nullptr;
}
