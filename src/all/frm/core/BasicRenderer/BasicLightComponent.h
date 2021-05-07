#pragma once

#include <frm/core/world/components/Component.h>

#include <EASTL/span.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// BasicLightComponent
// Basic analytical light type.
//
// \todo
// - Physically based units.
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(BasicLightComponent)
{
public:

	enum Type_
	{
		Type_Direct,
		Type_Point,
		Type_Spot,

		Type_Count
	};
	typedef int Type;

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);
	static eastl::span<BasicLightComponent*> GetActiveComponents();

	static BasicLightComponent* CreateDirect(const vec3& _color, float _brightness, bool _castShadows);
	static BasicLightComponent* CreatePoint(const vec3& _color, float _brightness, float _radius, bool _castShadows);
	static BasicLightComponent* CreateSpot(const vec3& _color, float _brightness, float _radius, float _coneInnerAngle, float _coneOuterAngle, bool _castShadows);

	Type  getType() const                                           { return m_type; }
	void  setType(Type _type)                                       { m_type = _type; }
	vec4  getColorBrightness() const                                { return m_colorBrightness; }
	void  setColorBrightness(const vec3& _color, float _brightness) { m_colorBrightness = vec4(_color, _brightness); }
	float getRadius() const                                         { return m_radius; }
	void  setRadius(float _radius)                                  { m_radius = _radius; }
	float getConeInnerAngle() const                                 { return m_coneInnerAngle; }
	void  setConeInnerAngle(float _radians)                         { m_coneInnerAngle = _radians; }
	float getConeOuterAngle() const                                 { return m_coneOuterAngle; }
	void  setConeOuterAngle(float _radians)                         { m_coneOuterAngle = _radians; }
	bool  getCastShadows() const                                    { return m_castShadows; }
	void  setCastShadows(bool _castShadows)                         { m_castShadows = _castShadows; }

protected:

	Type  m_type                   = Type_Direct;
	vec4  m_colorBrightness        = vec4(1.0f);
	float m_radius                 = 5.0f;            // Meters, controls linear attenuation.
	float m_coneInnerAngle         = Radians(1.0f);   // Radians, controls angular attenuation.
	float m_coneOuterAngle         = Radians(20.0f);  //                   "
	bool  m_castShadows            = false;

	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
	bool isStatic() override { return true; }

	friend class BasicRenderer;
};

} // namespace frm