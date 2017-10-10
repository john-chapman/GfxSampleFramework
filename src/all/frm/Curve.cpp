#include <frm/Curve.h>

using namespace frm;
using namespace apt;


// PUBLIC

Curve::Index Curve::insert(const Endpoint& _endpoint)
{
	Index ret = (Index)m_endpoints.size();
	if (!m_endpoints.empty() && _endpoint.m_value.x < m_endpoints[ret - 1].m_value.x) {
	 // can't insert at end, do binary search
		ret = findSegmentStart(_endpoint.m_value.x);
		ret += (_endpoint.m_value.x >= m_endpoints[ret].m_value.x) ? 1 : 0; // handle case where _pos.x should be inserted at 0, normally we +1 to ret
	}
	m_endpoints.insert(m_endpoints.begin() + ret, _endpoint);

	if (m_wrap == Wrap_Repeat) {
	 // synchronize first/last endpoints
		if (ret == (int)m_endpoints.size() - 1) {
			copyValueAndTangent(m_endpoints.back(), m_endpoints.front());
		} else if (ret == 0) {
			copyValueAndTangent(m_endpoints.front(), m_endpoints.back());
		}
	}

	updateExtentsAndConstrain(ret);

	return ret;
}

Curve::Index Curve::move(Index _endpoint, Component _component, const vec2& _value)
{
	Endpoint& ep = m_endpoints[_endpoint];

	Index ret = _endpoint;
	if (_component == Component_Value) {
	 // move CPs
		vec2 delta = _value - ep[Component_Value];
		ep[Component_In] += delta;
		ep[Component_Out] += delta;

	 // swap with neighbor
		ep.m_value = _value;
		if (delta.x > 0.0f && _endpoint < (Index)m_endpoints.size() - 1) {
			int i = _endpoint + 1;
			if (_value.x > m_endpoints[i].m_value.x) {
				eastl::swap(ep, m_endpoints[i]);
				ret = i;
			}
		} else if (_endpoint > 0) {
			int i = _endpoint - 1;
			if (_value.x < m_endpoints[i].m_value.x) {
				eastl::swap(ep, m_endpoints[i]);
				ret = i;
			}
		}

	} else {
	 // prevent crossing VP in x
		ep[_component] = _value;
		if (_component == Component_In) {
			ep[_component].x = min(ep[_component].x, ep[Component_Value].x);
		} else {
			ep[_component].x = max(ep[_component].x, ep[Component_Value].x);
		}

	 // CPs are locked so we must update the other
	 // \todo unlocked CPs?
		Component other = _component == Component_In ? Component_Out : Component_In;
		int i = other == Component_In ? 0 : 1;
		vec2 v = ep[Component_Value] - ep[_component];
		ep[other] = ep[Component_Value] + v;

	}

	if (m_wrap == Wrap_Repeat) {
	 // synchronize first/last endpoints
		if (ret == (int)m_endpoints.size() - 1) {
			copyValueAndTangent(m_endpoints.back(), m_endpoints.front());
		} else if (ret == 0) {
			copyValueAndTangent(m_endpoints.front(), m_endpoints.back());
		}
	}

	updateExtentsAndConstrain(ret);

	return ret;
}

void Curve::erase(Index _endpoint)
{
	APT_ASSERT(_endpoint < (Index)m_endpoints.size());
	m_endpoints.erase(m_endpoints.begin() + _endpoint);
	updateExtentsAndConstrain(APT_MIN(_endpoint, APT_MAX((Index)m_endpoints.size() - 1, 0)));
}

float Curve::wrap(float _t) const
{
	float ret = _t;
	switch (m_wrap) {
		case Wrap_Repeat:
			ret = ret - m_valueMin.x * floor(ret / (m_valueMax.x - m_valueMin.x));
			break;
		case Wrap_Clamp:
		default:
			ret = APT_CLAMP(ret, m_valueMin.x, m_valueMax.x);
			break;
	};
	APT_ASSERT(ret >= m_valueMin.x && ret <= m_valueMax.x);
	return ret;
}

// PRIVATE

Curve::Index Curve::findSegmentStart(float _t) const
{
	Index lo = 0, hi = (Index)m_endpoints.size() - 1;
	while (hi - lo > 1) {
		u32 md = (hi + lo) / 2;
		if (_t > m_endpoints[md].m_value.x) {
			lo = md;
		} else {
			hi = md;
		}
	}
	return _t > m_endpoints[hi].m_value.x ? hi : lo;
}

void Curve::updateExtentsAndConstrain(Index _endpoint)
{
	m_valueMin = m_endpointMin = vec2(FLT_MAX);
	m_valueMax = m_endpointMax = vec2(-FLT_MAX);
	for (auto& ep : m_endpoints) {
		m_valueMin = min(m_valueMin, ep.m_value);
		m_valueMax = min(m_valueMax, ep.m_value);
		for (int i = 0; i < Component_Count; ++i) {
			m_endpointMin = min(m_endpointMin, ep[i]);
			m_endpointMax = max(m_endpointMax, ep[i]);
		}
	}

	if (m_wrap == Wrap_Repeat) {
	 // synchronize first/last endpoints
		if (_endpoint == (int)m_endpoints.size() - 1) {
			copyValueAndTangent(m_endpoints.back(), m_endpoints.front());
		} else if (_endpoint == 0) {
			copyValueAndTangent(m_endpoints.front(), m_endpoints.back());
		}
	}
}

void Curve::copyValueAndTangent(const Endpoint& _src, Endpoint& dst_)
{
	dst_.m_value.y = _src.m_value.y;
	dst_.m_in  = dst_.m_value + (_src.m_in  - _src.m_value);
	dst_.m_out = dst_.m_value + (_src.m_out - _src.m_value);
}
