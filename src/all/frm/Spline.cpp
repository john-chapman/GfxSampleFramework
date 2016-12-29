#include <frm/Spline.h>

#include <frm/interpolation.h>
#include <frm/math.h>
#include <frm/Profiler.h>

#include <apt/Json.h>
#include <apt/Time.h>

#include <frm/Im3d.h>
#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

// PUBLIC

SplinePath::SplinePath()
	: itpl(itpl_cuberp)
{
}

vec3 SplinePath::evaluate(float _t) const
{
	CPU_AUTO_MARKER("Spline::evaluate");
	int i = findSegment(_t);
	const Segment& seg = m_segs[i];
	float t = (_t - seg.m_beg) / seg.m_len;
	int p0, p1, p2, p3;
	getClampIndices(i, p0, p1, p2, p3);
	return itpl(m_data[p0], m_data[p1], m_data[p2], m_data[p3], t);
}

float SplinePath::reparam(float _t) const
{
	float s = _t * m_length; // distance along the whole spline

	int lo = 0, hi = (int)m_usTable.size() - 2;	
	while (hi - lo > 1) {
		int mid = (hi + lo) / 2;		
		if (s > m_usTable[mid].y) {
			lo = mid;
		} else {
			hi = mid;
		}
	}	
	int seg = lo;
	if (s > m_usTable[hi].y) {
		seg = hi;
	}

	float d = (s - m_usTable[seg].y) / (m_usTable[seg + 1].y - m_usTable[seg].y);
	return lerp(m_usTable[seg].x, m_usTable[seg + 1].x, d);
}

vec3 SplinePath::evaluateLinear(float _t) const
{
	int lo = 0, hi = (int)m_subdiv.size() - 2;
	while (hi - lo > 1) {
		int mid = (hi + lo) / 2;		
		if (_t > m_subdiv[mid].w) {
			lo = mid;
		} else {
			hi = mid;
		}
	}	
	int seg = lo;
	if (_t > m_subdiv[hi].w) {
		seg = hi;
	}

	vec3 p0 = vec3(m_subdiv[seg]);
	vec3 p1 = vec3(m_subdiv[seg + 1]);
	float t = (_t - m_subdiv[seg].w) / (m_subdiv[seg + 1].w - m_subdiv[seg].w);
	return lerp(p0, p1, t);
}

void SplinePath::append(const vec3& _position)
{
	m_data.push_back(_position);
}

void SplinePath::build()
{
	APT_AUTOTIMER_DBG("SplinePath::build");
	if (m_data.empty()) {
		return;
	}
	m_segs.clear();
	m_segs.resize(m_data.size() - 1);

 // segment beg/length, total length
	m_length = 0.0f;
	for (int i = 0, n = (int)m_segs.size(); i < n; ++i) {
		int p0, p1, p2, p3;
		getClampIndices(i, p0, p1, p2, p3);
		m_segs[i].m_len = arclen(m_data[p0], m_data[p1], m_data[p2], m_data[p3]);
		m_length += m_segs[i].m_len;
		m_segs[i].m_beg = i == 0 ? 0.0f : m_segs[i - 1].m_beg + m_segs[i - 1].m_len;
	}

 // normalize
	for (int i = 0, n = (int)m_segs.size(); i < n; ++i) {
		m_segs[i].m_beg /= m_length;
		m_segs[i].m_len /= m_length;
	}

 // us table
	m_usTable.clear();
	for (int i = 0, n = (int)m_data.size() * 10; i < n; ++i) {
		vec2 us;
		us.x = (float)i / (float)n;
		us.y = arclen(0.0f, us.x);
		m_usTable.push_back(us);
	}

 // subdiv
	m_subdiv.clear();
	for (int i = 0, n = (int)m_data.size() - 1; i < n; ++i) {
		subdiv(i);
	}
	m_length = 0.0f;
	m_subdiv[0].w = 0.0f;
	for (int i = 1, n = (int)m_subdiv.size() - 1; i < n; ++i) {
		float seglen = length(vec3(m_subdiv[i]) - vec3(m_subdiv[i - 1]));
		m_length += seglen;
		m_subdiv[i].w = m_subdiv[i - 1].w + seglen;
	}
	for (int i = 1, n = (int)m_subdiv.size(); i < n; ++i) {
		m_subdiv[i].w /= m_length;
	}
}

void SplinePath::edit()
{
	static const Im3d::Color kColorPath   = Im3d::Color(0.0f, 1.0f, 0.2f, 0.8f);
	static const Im3d::Color kColorPoints = Im3d::Color(1.0f, 1.0f, 1.0f, 1.0f);
	static const int kPathDetail = 16;

	ImGui::Text("%d CPs, subdiv = %d", (int)m_data.size(), (int)m_subdiv.size());

	static int s_itpl = 1;
	if (ImGui::Combo("Interpolation", &s_itpl, "Lerp\0Cuberp\0Hermite\0")) {
		switch (s_itpl) {
			case 0:  itpl = itpl_lerp; break;
			case 1:
			default: itpl = itpl_cuberp; break;
			case 2:  itpl = itpl_hermite; break;
		};
		build();
	}

	Im3d::PushDrawState();
		Im3d::SetColor(kColorPath);
		Im3d::SetSize(2.0f);
		Im3d::BeginLineStrip();
			/*for (int i = 0, n = (int)m_data.size() - 1; i < n; ++i) {
				for (int j = 0; j <= kPathDetail; ++j) {
					float t = (float)j / (float)kPathDetail;
					int p0, p1, p2, p3;
					getClampIndices(i, p0, p1, p2, p3);
					vec3 p = itpl(m_data[p0], m_data[p1], m_data[p2], m_data[p3], t);
					Im3d::Vertex(p);
				}
			}*/
			for (int i = 0, n = (int)m_subdiv.size(); i < n; ++i) {
				Im3d::Vertex(vec3(m_subdiv[i]));
			}
		Im3d::End();
		
		Im3d::SetSize(4.0f);
		Im3d::SetColor(kColorPoints);
		Im3d::BeginPoints();
			for (int i = 0, n = (int)m_data.size(); i < n; ++i) {
				Im3d::Vertex(m_data[i]);
			 // \todo edit handles here
				//if (Im3d::GizmoPosition("SplinePath_EditData", &m_data[1])) {
				//	build();
				//}
			}
		Im3d::End();
if (Im3d::GizmoPosition("SplinePath_EditData", &m_data[1])) {
	build();
}
		static bool s_reparam = false;
		static bool s_linear = true;
		static float s_t = 0.0f;
		static vec3 pp = vec3(0.0f);
		vec3 p;
		if (s_reparam) {
			float rpt = reparam(s_t);
			p = evaluate(rpt);
			ImGui::Text("Reparam T %f", rpt);
		} else {
			p = evaluate(s_t);
		}
		if (s_linear) {
			p = evaluateLinear(s_t);
		}
		ImGui::Text("Velocity %f", length(p - pp));
		pp = p;
		s_t += (1.0f/60.0f) / 100.0f;
		if (s_t > 1.0f) s_t = 0.0f;
		ImGui::SliderFloat("T", &s_t, 0.0f, 1.0f);
		ImGui::Checkbox("Constant Speed", &s_reparam);
		ImGui::Checkbox("Use Subdiv", &s_linear);
		Im3d::SetColor(1.0f, 0.0f, 1.0f, 1.0f);
		Im3d::SetSize(10.0f);
		Im3d::BeginPoints();
			Im3d::Vertex(p);
		Im3d::End();

		if (ImGui::TreeNode("arclen")) {
			ImGui::Text("Total Length: %f", m_length);
			static float u0 = 0.0f;
			static float u1 = 1.0f;
			//ImGui::SliderFloat("u0", &u0, 0.0f, 1.0f);
			ImGui::SliderFloat("u1", &u1, 0.0f, 1.0f);
			ImGui::Text("[u0,u1] %f", arclen(0.0f, u1));
			Im3d::SetSize(8.0f);
			Im3d::BeginPoints();
				//Im3d::Vertex(evaluate(u0));
				Im3d::Vertex(evaluate(u1));
			Im3d::End();
			ImGui::TreePop();
		}

	Im3d::PopDrawState();
}

// PRIVATE

int SplinePath::findSegment(float _t) const
{
	int lo = 0, hi = (int)m_segs.size() - 1;
	while (hi - lo > 1) {
		int mid = (hi + lo) / 2;
		if (_t > m_segs[mid].m_beg) {
			lo = mid;
		} else {
			hi = mid;
		}
	}
	if (_t > m_segs[hi].m_beg) {
		return hi;
	}
	return lo;
}

vec3 SplinePath::itpl_lerp(const vec3& _p0, const vec3& _p1, const vec3& _p2, const vec3& _p3, float _t)
{
	return lerp(_p1, _p2, _t);
}

vec3 SplinePath::itpl_cuberp(const vec3& _p0, const vec3& _p1, const vec3& _p2, const vec3& _p3, float _t)
{
	return cuberp(_p0, _p1, _p2, _p3, _t);
}

vec3 SplinePath::itpl_hermite(const vec3& _p0, const vec3& _p1, const vec3& _p2, const vec3& _p3, float _t)
{
	return hermite(_p0, _p1, _p2, _p3, _t);
}

void SplinePath::subdiv(int _segment, float _t0, float _t1, float _maxError, int _limit)
{
	int i0, i1, i2, i3;
	getClampIndices(_segment, i0, i1, i2, i3);

	vec3 beg = itpl(m_data[i0], m_data[i1], m_data[i2], m_data[i3], _t0);
	vec3 end = itpl(m_data[i0], m_data[i1], m_data[i2], m_data[i3], _t1);
	if (_limit == 0) {
		m_subdiv.push_back(vec4(beg, 0.0f));
		m_subdiv.push_back(vec4(end, 0.0f));
		return;
	}
	--_limit;

	float tm = (_t0 + _t1) * 0.5f;
	vec3 mid = itpl(m_data[i0], m_data[i1], m_data[i2], m_data[i3], tm);
	float a = length(mid - beg);
	float b = length(end - mid);
	float c = length(end - beg);
	if ((a + b) - c < _maxError) {
		m_subdiv.push_back(vec4(beg, 0.0f));
		m_subdiv.push_back(vec4(end, 0.0f));
		return;
	}

	subdiv(_segment, _t0, tm, _maxError, _limit);
	subdiv(_segment, tm, _t1, _maxError, _limit);
}

float SplinePath::arclen(
	const vec3& _p0, const vec3& _p1, const vec3& _p2, const vec3& _p3,
	float _threshold,
	float _tbeg, float _tmid, float _tend,
	float _step
	) const
{
	vec3 beg = itpl(_p0, _p1, _p2, _p3, _tbeg);
	vec3 mid = itpl(_p0, _p1, _p2, _p3, _tmid);
	vec3 end = itpl(_p0, _p1, _p2, _p3, _tend);

	float a = length(mid - beg);
	float b = length(end - mid);
	float c = length(end - beg);
	if (((a + b) - c) < _threshold) {
		return c;
	}

	return 
		arclen(_p0, _p1, _p2, _p3, _threshold, _tbeg, _tmid - _step, _tmid, _step * 0.5f) +
		arclen(_p0, _p1, _p2, _p3, _threshold, _tmid, _tmid + _step, _tend, _step * 0.5f)
		;
}

float SplinePath::arclen(float _t0, float _t1, float _threshold) const
{
	APT_ASSERT(m_segs.size() == m_data.size() - 1); // must call build() first
	float ret = 0.0f;

	int seg0 = findSegment(_t0);
	int seg1 = findSegment(_t1);

 // sum whole seg lengths
	for (int i = seg0 + 1; i < seg1; ++i) {
		ret += m_segs[i].m_len * m_length;
	}

 // find partial seg lengths
	int p0, p1, p2, p3;
	getClampIndices(seg0, p0, p1, p2, p3);
	float u0 = (_t0 - m_segs[p1].m_beg) / m_segs[p1].m_len;
	if (seg0 == seg1) {
		float u1 = (_t1 - m_segs[p1].m_beg) / m_segs[p1].m_len;
		ret += arclen(
			m_data[p0], m_data[p1], m_data[p2], m_data[p3],
			_threshold,
			u0, (u0 + u1) * 0.5f, u1,
			(u1 - u0) * 0.25f
			);
	} else {
		ret += arclen(
			m_data[p0], m_data[p1], m_data[p2], m_data[p3],
			_threshold,
			u0, (u0 + 1.0f) * 0.5f, 1.0f,
			(1.0f - u0) * 0.25f
			);
	
		getClampIndices(seg1, p0, p1, p2, p3);
		float u1 = (_t1 - m_segs[p1].m_beg) / m_segs[p1].m_len;
		ret += arclen(
			m_data[p0], m_data[p1], m_data[p2], m_data[p3],
			_threshold,
			0.0f, u1 * 0.5f, u1,
			(u1 - 0.0f) * 0.25f
			);
	}

	return ret;
}

void SplinePath::getClampIndices(int _i, int& p0_, int& p1_, int& p2_, int& p3_) const
{
	p0_ = APT_MAX(_i - 1, 0);
	p1_ = _i;
	p2_ = APT_MIN(_i + 1, (int)m_data.size() - 1);
	p3_ = APT_MIN(_i + 2, (int)m_data.size() - 1);
}
