#pragma once
#ifndef md5mesh_h
#define md5mesh_h

#include <apt/def.h>
#include <apt/log.h>
#include <apt/math.h>
#include <apt/String.h>
#include <apt/TextParser.h>

#include <EASTL/vector.h>

namespace md5 {

enum PositionComponents
{
	Position_X = (1 << 0),
	Position_Y = (1 << 1),
	Position_Z = (1 << 2),
	Position_Count,
	Position_Mask = Position_X + Position_Y + Position_Z
};
enum OrientationComponents
{
	Orientation_X = (1 << 3),
	Orientation_Y = (1 << 4),
	Orientation_Z = (1 << 5),
	Orientation_Count,
	Orientation_Mask = Orientation_X + Orientation_Y + Orientation_Z
};

typedef apt::String<32> NameStr;

struct MeshJoint {
	NameStr    m_name;
	long int   m_parentIndex;
	apt::vec3  m_position;
	apt::quat  m_orientation;
};

struct AnimJoint {
	NameStr    m_name;
	long int   m_parentIndex;
	long int   m_flags;
	long int   m_startIndex;
	apt::vec3  m_position;	  // from the base frame
	apt::quat  m_orientation; //         "
};

struct Bounds {
	apt::vec3  m_min;
	apt::vec3  m_max;
};

struct Vert {
	apt::vec2  m_texcoord;
	long int   m_weightStart;
	long int   m_weightCount;
};

struct Tri {
	long int   m_verts[3];
};

struct Weight {
	long int   m_jointIndex;
	float      m_bias;
	apt::vec3  m_position;
};

struct Mesh {
	NameStr               m_shader;
	eastl::vector<Vert>   m_verts;
	eastl::vector<Tri>    m_tris;
	eastl::vector<Weight> m_weights;
};

#define md5_syntax_err(_msg) \
	APT_LOG_ERR("md5 syntax error, line %d: %s", _tp_.getLineCount(), _msg); \
	return false

#define md5_err(_fmt, ...) \
	APT_LOG_ERR("md5 error: " _fmt, __VA_ARGS__); \
	return false

#define md5_call(_func) \
	if (!(_func)) return false;

inline void SkipWhitespaceOrComment(apt::TextParser& _tp_)
{
	for (;;) {
		_tp_.skipWhitespace();
		if (_tp_[0] == '/' && _tp_[1] == '/') {
			_tp_.skipLine();
		} else {
			break;
		}
	}
}

inline bool ParseString(apt::TextParser& _tp_, NameStr& out_)
{
	_tp_.skipWhitespace();
	if (*_tp_ != '"') {
		md5_syntax_err("expected '\"'");
	}
	_tp_.advance(); // skip "
	const char* beg = _tp_;
	if (_tp_.advanceToNext('"') != '"') {
		md5_syntax_err("expected '\"'");
	}
	out_.set(beg, _tp_ - beg);
	_tp_.advance(); // skip "
	return true;
}

inline bool ParseFloat(apt::TextParser& _tp_, float* out_)
{
	_tp_.skipWhitespace();
	double d;
	if (!_tp_.readNextDouble(d)) {
		md5_syntax_err("expected a number");
	}
	*out_ = (float)d;
	return true;
}

inline bool ParseInt(apt::TextParser& _tp_, long int* out_)
{
	_tp_.skipWhitespace();
	if (!_tp_.readNextInt(*out_)) {
		md5_syntax_err("expected a number");
	}
	return true;
}

inline bool ParseFloatArray(apt::TextParser& _tp_, int count_, float out_[])
{
	_tp_.skipWhitespace();
	if (*_tp_ != '(') {
		md5_syntax_err("expected '('");
	}
	_tp_.advance(); // skip (
	for (int i = 0; i < count_; ++i) {
		md5_call(ParseFloat(_tp_, &out_[i]));
	}
	_tp_.skipWhitespace();
	if (*_tp_ != ')') { 
		md5_syntax_err("expected ')'");
	}
	_tp_.advance(); // skip )
	return true;
}

inline bool ParseMeshJoint(apt::TextParser& _tp_, MeshJoint& out_)
{
	SkipWhitespaceOrComment(_tp_);
	md5_call(ParseString(_tp_, out_.m_name));
	md5_call(ParseInt(_tp_, &out_.m_parentIndex));
	md5_call(ParseFloatArray(_tp_, 3, &out_.m_position.x));
	md5_call(ParseFloatArray(_tp_, 3, &out_.m_orientation.x));
	SkipWhitespaceOrComment(_tp_);

 // recover orientation w
	float t = 1.0f - length2(out_.m_orientation);
	out_.m_orientation.w = t < 0.0f ? 0.0f : -sqrtf(t);

	return true;
}

inline bool ParseAnimJoint(apt::TextParser& _tp_, AnimJoint& out_)
{
	SkipWhitespaceOrComment(_tp_);
	md5_call(ParseString(_tp_, out_.m_name));
	md5_call(ParseInt(_tp_, &out_.m_parentIndex));
	md5_call(ParseInt(_tp_, &out_.m_flags));
	md5_call(ParseInt(_tp_, &out_.m_startIndex));
	SkipWhitespaceOrComment(_tp_);
	return true;
}

inline bool ParseAnimJointPositionOrientation(apt::TextParser& _tp_, AnimJoint& out_)
{
	SkipWhitespaceOrComment(_tp_);
	md5_call(ParseFloatArray(_tp_, 3, &out_.m_position.x));
	md5_call(ParseFloatArray(_tp_, 3, &out_.m_orientation.x));
	SkipWhitespaceOrComment(_tp_);

 // recover orientation w
	float t = 1.0f - length2(out_.m_orientation);
	out_.m_orientation.w = t < 0.0f ? 0.0f : -sqrtf(t);

	return true;
}

inline bool ParseAnimJointList(apt::TextParser& _tp_, long int _numJoints, AnimJoint out_[])
{
	if (_tp_.advanceToNext('{') != '{') {
		md5_syntax_err("expected '{'");
	}
	_tp_.advance(); // skip {
	SkipWhitespaceOrComment(_tp_);

	long int jointCount = 0;
	while (!_tp_.isNull() && *_tp_ != '}') {
		if (jointCount > _numJoints) {
			md5_err("too many joints, expected %d", _numJoints);
		}
		md5_call(ParseAnimJoint(_tp_, out_[jointCount++]));
	}
	if (jointCount < _numJoints) {
		md5_err("too few joints, expected %d", _numJoints);
	}
	if (*_tp_ != '}') {
		md5_syntax_err("expected '}'");
	}
	_tp_.advance(); // skip }
	SkipWhitespaceOrComment(_tp_);
	return true;
}
inline bool ParseBaseFrame(apt::TextParser& _tp_, long int _numJoints, AnimJoint out_[])
{
	if (_tp_.advanceToNext('{') != '{') {
		md5_syntax_err("expected '{'");
	}
	_tp_.advance(); // skip {
	SkipWhitespaceOrComment(_tp_);

	long int jointCount = 0;
	while (!_tp_.isNull() && *_tp_ != '}') {
		if (jointCount > _numJoints) {
			md5_err("too many joints, expected %d", _numJoints);
		}
		md5_call(ParseAnimJointPositionOrientation(_tp_, out_[jointCount++]));
	}
	if (jointCount < _numJoints) {
		md5_err("too few joints, expected %d", _numJoints);
	}
	if (*_tp_ != '}') {
		md5_syntax_err("expected '}'");
	}
	_tp_.advance(); // skip }
	SkipWhitespaceOrComment(_tp_);
	return true;
}

inline bool ParseBounds(apt::TextParser& _tp_, Bounds& out_)
{
	SkipWhitespaceOrComment(_tp_);
	md5_call(ParseFloatArray(_tp_, 3, &out_.m_min.x))
	md5_call(ParseFloatArray(_tp_, 3, &out_.m_max.x))
	SkipWhitespaceOrComment(_tp_);
	return true;
}

inline bool ParseBoundsList(apt::TextParser& _tp_, long int _numBounds, Bounds out_[])
{
	if (_tp_.advanceToNext('{') != '{') {
		md5_syntax_err("expected '{'");
	}
	_tp_.advance(); // skip {
	SkipWhitespaceOrComment(_tp_);

	long int boundsCount = 0;
	while (!_tp_.isNull() && *_tp_ != '}') {
		if (boundsCount > _numBounds) {
			md5_err("too many bounds, expected %d", _numBounds);
		}
		md5_call(ParseBounds(_tp_, out_[boundsCount++]));
	}
	if (boundsCount < _numBounds) {
		md5_err("too few bounds, expected %d", _numBounds);
	}
	if (*_tp_ != '}') {
		md5_syntax_err("expected '}'");
	}
	_tp_.advance(); // skip }
	SkipWhitespaceOrComment(_tp_);
	return true;
}

inline bool ParseVert(apt::TextParser& _tp_, Vert out_[])
{
	if (!_tp_.compareNext("vert")) {
		md5_syntax_err("expected 'vert'");
	}
	long int i;
	md5_call(ParseInt(_tp_, &i));
	Vert& v = out_[i];
	md5_call(ParseFloatArray(_tp_, 2, &v.m_texcoord.x));
	md5_call(ParseInt(_tp_, &v.m_weightStart));
	md5_call(ParseInt(_tp_, &v.m_weightCount));
	SkipWhitespaceOrComment(_tp_);
	return true;
}

inline bool ParseTri(apt::TextParser& _tp_, Tri out_[])
{
	if (!_tp_.compareNext("tri")) {
		md5_syntax_err("expected 'tri'");
	}
	long int i;
	md5_call(ParseInt(_tp_, &i));
	Tri& t = out_[i];
	md5_call(ParseInt(_tp_, &t.m_verts[0]));
	md5_call(ParseInt(_tp_, &t.m_verts[1]));
	md5_call(ParseInt(_tp_, &t.m_verts[2]));
	SkipWhitespaceOrComment(_tp_);
	return true;
}

inline bool ParseWeight(apt::TextParser& _tp_, Weight out_[])
{
	if (!_tp_.compareNext("weight")) {
		md5_syntax_err("expected 'weight'");
	}
	long int i;
	md5_call(ParseInt(_tp_, &i));
	Weight& w = out_[i];
	md5_call(ParseInt(_tp_, &w.m_jointIndex));
	md5_call(ParseFloat(_tp_, &w.m_bias));
	md5_call(ParseFloatArray(_tp_, 3, &w.m_position.x));
	SkipWhitespaceOrComment(_tp_);
	return true;
}

inline bool ParseVersion(apt::TextParser& _tp_)
{
	if (!_tp_.compareNext("MD5Version")) {
		md5_syntax_err("expected 'MD5Version'");
	}
	long int version;
	md5_call(ParseInt(_tp_, &version));
	if (version != 10) {
		md5_err("version is %d, only version 10 supported", version);
	}
	_tp_.skipLine();
	return true;
}

inline bool ParseMeshHeader(apt::TextParser& _tp_, long int* numJoints_, long int* numMeshes_)
{
	md5_call(ParseVersion(_tp_));
	while (true) {
		const char* pos = _tp_;
		SkipWhitespaceOrComment(_tp_);
		if (_tp_.compareNext("commandline")) {
			_tp_.skipLine();
			continue;
		}
		if (_tp_.compareNext("numJoints")) {
			md5_call(ParseInt(_tp_, numJoints_));
			_tp_.skipLine();
			continue;
		}
		if (_tp_.compareNext("numMeshes")) {
			md5_call(ParseInt(_tp_, numMeshes_));
			_tp_.skipLine();
			continue;
		}

		if (_tp_.compareNext("joints")) {
			_tp_.reset(pos);
			break;
		}
		if (_tp_.compareNext("mesh")) {
			_tp_.reset(pos);
			break;
		}
	}
	return true;
}

inline bool ParseAnimHeader(apt::TextParser& _tp_, long int* numJoints_, long int* numFrames_, long int* frameRate_, long int* numAnimatedComponents_)
{
	md5_call(ParseVersion(_tp_));
	while (true) {
		const char* pos = _tp_;
		SkipWhitespaceOrComment(_tp_);
		if (_tp_.compareNext("commandline")) {
			_tp_.skipLine();
			continue;
		}
		if (_tp_.compareNext("numJoints")) {
			md5_call(ParseInt(_tp_, numJoints_));
			_tp_.skipLine();
			continue;
		}
		if (_tp_.compareNext("numFrames")) {
			md5_call(ParseInt(_tp_, numFrames_));
			_tp_.skipLine();
			continue;
		}
		if (_tp_.compareNext("frameRate")) {
			md5_call(ParseInt(_tp_, frameRate_));
			_tp_.skipLine();
			continue;
		}
		if (_tp_.compareNext("numAnimatedComponents")) {
			md5_call(ParseInt(_tp_, numAnimatedComponents_));
			_tp_.skipLine();
			continue;
		}

		if (_tp_.compareNext("hierarchy")) {
			_tp_.reset(pos);
			break;
		}
		if (_tp_.compareNext("bounds")) {
			_tp_.reset(pos);
			break;
		}
		if (_tp_.compareNext("baseframe")) {
			_tp_.reset(pos);
			break;
		}
		if (_tp_.compareNext("frame")) {
			_tp_.reset(pos);
			break;
		}
	}
	return true;
}

inline bool ParseFrame(apt::TextParser& _tp_, long int _numAnimatedComponents, float out_[])
{
	if (_tp_.advanceToNext('{') != '{') {
		md5_syntax_err("expected '{'");
	}
	_tp_.advance(); // skip {
	SkipWhitespaceOrComment(_tp_);

	if (_numAnimatedComponents > 0) { // can be if the animation is just a single pose
		int count = 0;
		while (!_tp_.isNull() && *_tp_ != '}') {
			if (count > _numAnimatedComponents) {
				md5_err("too many components, expected %d", _numAnimatedComponents);
			}
			md5_call(ParseFloat(_tp_, &out_[count]));
			_tp_.skipWhitespace();
			++count;
		}
		if (count < _numAnimatedComponents) {
			md5_err("too few components, expected %d", _numAnimatedComponents);
		}
	} else {
		_tp_.advanceToNext('}');
	}

	if (*_tp_ != '}') {
		md5_syntax_err("expected '}'");
	}
	_tp_.advance(); // skip }
	SkipWhitespaceOrComment(_tp_);
	return true;
}


inline bool ParseMeshJointList(apt::TextParser& _tp_, long int _numJoints, MeshJoint out_[])
{
	if (_tp_.advanceToNext('{') != '{') {
		md5_syntax_err("expected '{'");
	}
	_tp_.advance(); // skip {
	SkipWhitespaceOrComment(_tp_);

	long int jointCount = 0;
	while (!_tp_.isNull() && *_tp_ != '}') {
		if (jointCount > _numJoints) {
			md5_err("too many joints, expected %d", _numJoints);
		}
		md5_call(ParseMeshJoint(_tp_, out_[jointCount++]));
	}
	if (jointCount < _numJoints) {
		md5_err("too few joints, expected %d", _numJoints);
	}
	if (*_tp_ != '}') {
		md5_syntax_err("expected '}'");
	}
	_tp_.advance(); // skip }
	SkipWhitespaceOrComment(_tp_);
	return true;
}

inline bool ParseMesh(apt::TextParser& _tp_, Mesh& out_)
{
	if (_tp_.advanceToNext('{') != '{') {
		md5_syntax_err("expected '{'");
	}
	_tp_.advance(); // skip {
	SkipWhitespaceOrComment(_tp_);

	if (!_tp_.compareNext("shader")) {
		md5_syntax_err("expected 'shader'");
	}
	md5_call(ParseString(_tp_, out_.m_shader));
	SkipWhitespaceOrComment(_tp_);

	while (!_tp_.isNull() && *_tp_ != '}') {
		if (_tp_.compareNext("numverts")) {
			long int numverts;
			md5_call(ParseInt(_tp_, &numverts));
			_tp_.skipLine();
			out_.m_verts.resize(numverts);
			for (long int i = 0; i < numverts; ++i) {
				md5_call(ParseVert(_tp_, out_.m_verts.data()));
			}
			continue;
		}

		if (_tp_.compareNext("numtris")) {
			long int numtris;
			md5_call(ParseInt(_tp_, &numtris));
			_tp_.skipLine();
			out_.m_tris.resize(numtris);
			for (long int i = 0; i < numtris; ++i) {
				md5_call(ParseTri(_tp_, out_.m_tris.data()));
			}
			continue;
		}

		if (_tp_.compareNext("numweights")) {
			long int numweights;
			md5_call(ParseInt(_tp_, &numweights));
			_tp_.skipLine();
			out_.m_weights.resize(numweights);
			for (long int i = 0; i < numweights; ++i) {
				md5_call(ParseWeight(_tp_, out_.m_weights.data()));
			}
			continue;
		}
	}

	if (*_tp_ != '}') {
		md5_syntax_err("expected '}'");
	}
	_tp_.advance(); // skip }
	SkipWhitespaceOrComment(_tp_);
	return true;
}


} // namespace md5


#endif // md5mesh_h