#pragma once

#include <frm/core/world/components/Component.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/span.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// BasicRenderableComponent
//
// \todo 
// - Serialize materials inline if they don't have a path.
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(BasicRenderableComponent)
{
public:

	static void  Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);
	static eastl::span<BasicRenderableComponent*> GetActiveComponents();

	static BasicRenderableComponent* Create(
		Mesh* _mesh,
		BasicMaterial* _material
		);


	void         setPose(const Skeleton& _skeleton);
	void         clearPose();

	bool         getCastShadows() const                   { return m_castShadows; }
	void         setCastShadows(bool _castShadows)        { m_castShadows = _castShadows; }
	const vec4&  getColorAlpha() const                    { return m_colorAlpha; }
	void         setColorAlpha(const vec4& _colorAlpha)   { m_colorAlpha = _colorAlpha; }
	void         setColor(const vec3& _color)             { m_colorAlpha = vec4(_color, m_colorAlpha.w); }
	vec3         getColor() const                         { return m_colorAlpha.xyz(); }
	void         setAlpha(float _alpha)                   { m_colorAlpha.w = _alpha; }
	float        getAlpha() const                         { return m_colorAlpha.w; }
	const mat4&  getPrevWorld() const                     { return m_prevWorld; }
	void         setPrevWorld(const mat4& _prevWorld)     { m_prevWorld = _prevWorld; }

protected:

	bool         initImpl() override;
	bool         postInitImpl() override;
	void         shutdownImpl() override;
	bool         editImpl() override;
	bool         serializeImpl(Serializer& _serializer_) override;
	bool         isStatic() override { return true; }

	bool                                   m_castShadows = true;
	vec4                                   m_colorAlpha  = vec4(1.0f);
	mat4                                   m_world       = identity;
	mat4                                   m_prevWorld   = identity;
	Mesh*                                  m_mesh        = nullptr;
	PathStr                                m_meshPath    = "";
	eastl::fixed_vector<BasicMaterial*, 1> m_materials;      // per submesh
	eastl::fixed_vector<PathStr, 1>        m_materialPaths;  //     "
	eastl::fixed_vector<mat4, 1>           m_pose;
	eastl::fixed_vector<mat4, 1>           m_prevPose;

	friend class BasicRenderer;
};

} // namespace frm
