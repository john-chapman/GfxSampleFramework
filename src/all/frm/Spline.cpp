#include <frm/Spline.h>

#include <frm/interpolation.h>
#include <frm/math.h>

#include <apt/Json.h>
#include <apt/Time.h>

#include <frm/Im3d.h>
#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

// PUBLIC

SplinePath::SplinePath()
{
}

vec3 SplinePath::evaluate(float _t, int* _hint_) const
{
	int seg;
	if (_hint_ == nullptr) { // no hint, use binary search
		int lo = 0, hi = (int)m_eval.size() - 2;
		while (hi - lo > 1) {
			int mid = (hi + lo) / 2;		
			if (_t > m_eval[mid].w) {
				lo = mid;
			} else {
				hi = mid;
			}
		}	
		seg = lo;
		if (_t > m_eval[hi].w) {
			seg = hi;
		}

	} else { // hint, use linear search
		seg = *_hint_;
		while (_t > m_eval[seg + 1].w) {
			seg = (seg + 1) % (int)m_eval.size();
		}
		*_hint_ = seg;
	}

	vec3 p0 = vec3(m_eval[seg]);
	vec3 p1 = vec3(m_eval[seg + 1]);
	float t = (_t - m_eval[seg].w) / (m_eval[seg + 1].w - m_eval[seg].w);
	return lerp(p0, p1, t);
}

void SplinePath::append(const vec3& _position)
{
	m_raw.push_back(_position);
}

void SplinePath::build()
{
	APT_AUTOTIMER_DBG("SplinePath::build");
	if (m_raw.empty()) {
		return;
	}
	m_eval.clear();
	for (int i = 0, n = (int)m_raw.size() - 1; i < n; ++i) {
		subdiv(i);
	}
	m_length = 0.0f;
	m_eval[0].w = 0.0f;
	for (int i = 1, n = (int)m_eval.size() - 1; i < n; ++i) {
		float seglen = length(vec3(m_eval[i]) - vec3(m_eval[i - 1]));
		m_length += seglen;
		m_eval[i].w = m_eval[i - 1].w + seglen;
	}
	for (int i = 1, n = (int)m_eval.size(); i < n; ++i) {
		m_eval[i].w /= m_length;
	}
}

void SplinePath::edit()
{
	static const Im3d::Color kColorPath   = Im3d::Color(0.0f, 1.0f, 0.2f, 0.8f);
	static const Im3d::Color kColorPoints = Im3d::Color(1.0f, 1.0f, 1.0f, 1.0f);
	static const int kPathDetail = 16;

	ImGui::Text("Raw: %d, Eval: %d", (int)m_raw.size(), (int)m_eval.size());
	ImGui::Text("Length: %f", m_length);

	Im3d::PushDrawState();
		Im3d::SetColor(kColorPath);
		Im3d::SetSize(2.0f);
		Im3d::BeginLineStrip();
			for (int i = 0, n = (int)m_eval.size(); i < n; ++i) {
				Im3d::Vertex(vec3(m_eval[i]));
			}
		Im3d::End();
		
		Im3d::SetSize(4.0f);
		Im3d::SetColor(kColorPoints);
		Im3d::BeginPoints();
			for (int i = 0, n = (int)m_raw.size(); i < n; ++i) {
				Im3d::Vertex(m_raw[i]);
			 // \todo edit handles here
				//if (Im3d::GizmoPosition("SplinePath_EditData", &m_raw[1])) {
				//	build();
				//}
			}
		Im3d::End();

	Im3d::PopDrawState();
}

bool SplinePath::serialize(JsonSerializer& _serializer_)
{
	if (_serializer_.beginArray("Raw")) {
		if (_serializer_.getMode() == JsonSerializer::kRead) {
			m_raw.clear();
			m_raw.reserve(_serializer_.getArrayLength());
			vec3 p;
			while (_serializer_.value(p)) {
				m_raw.push_back(p);
			}
		} else {
			for (auto& p : m_raw) {
				_serializer_.value(p);
			}
		}
		_serializer_.endArray();
	}

	if (_serializer_.getMode() == JsonSerializer::kRead) {
		build();
	}
	return true;
}

// PRIVATE

void SplinePath::subdiv(int _segment, float _t0, float _t1, float _maxError, int _limit)
{
	int i0, i1, i2, i3;
	getClampIndices(_segment, i0, i1, i2, i3);

	vec3 beg = cuberp(m_raw[i0], m_raw[i1], m_raw[i2], m_raw[i3], _t0);
	vec3 end = cuberp(m_raw[i0], m_raw[i1], m_raw[i2], m_raw[i3], _t1);
	if (_limit == 0) {
		m_eval.push_back(vec4(beg, 0.0f));
		m_eval.push_back(vec4(end, 0.0f));
		return;
	}
	--_limit;

	float tm = (_t0 + _t1) * 0.5f;
	vec3 mid = cuberp(m_raw[i0], m_raw[i1], m_raw[i2], m_raw[i3], tm);
	float a = length(mid - beg);
	float b = length(end - mid);
	float c = length(end - beg);
	if ((a + b) - c < _maxError) {
		m_eval.push_back(vec4(beg, 0.0f));
		m_eval.push_back(vec4(end, 0.0f));
		return;
	}

	subdiv(_segment, _t0, tm, _maxError, _limit);
	subdiv(_segment, tm, _t1, _maxError, _limit);
}

void SplinePath::getClampIndices(int _i, int& i0_, int& i1_, int& i2_, int& i3_) const
{
	i0_ = APT_MAX(_i - 1, 0);
	i1_ = _i;
	i2_ = APT_MIN(_i + 1, (int)m_raw.size() - 1);
	i3_ = APT_MIN(_i + 2, (int)m_raw.size() - 1);
}
