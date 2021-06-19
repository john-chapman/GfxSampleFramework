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

	enum class Flag
	{
		CastShadows,         // Cast shadows.
		VisibleToEnvProbes,  // If static, visible to environment probes.

		BIT_FLAGS_COUNT_DEFAULT(CastShadows, VisibleToEnvProbes)
	};
	using Flags = BitFlags<Flag>;

	static void  Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);
	static eastl::span<BasicRenderableComponent*> GetActiveComponents();

	static BasicRenderableComponent* Create(DrawMesh* _mesh, BasicMaterial* _material);

	DrawMesh*    getMesh()                                { return m_mesh; }
	void         setMesh(DrawMesh* _mesh);
	void         setMaterial(BasicMaterial* _material, int _submeshIndex = 0);

	void         setPose(const Skeleton& _skeleton);
	void         clearPose();

	void         setFlag(Flag _flag, bool _value);
	bool         getFlag(Flag _flag) const                { return m_flags.get(_flag); }
	const vec4&  getColorAlpha() const                    { return m_colorAlpha; }
	void         setColorAlpha(const vec4& _colorAlpha)   { m_colorAlpha = _colorAlpha; }
	void         setColor(const vec3& _color)             { m_colorAlpha = vec4(_color, m_colorAlpha.w); }
	vec3         getColor() const                         { return m_colorAlpha.xyz(); }
	void         setAlpha(float _alpha)                   { m_colorAlpha.w = _alpha; }
	float        getAlpha() const                         { return m_colorAlpha.w; }
	const mat4&  getPrevWorld() const                     { return m_prevWorld; }
	void         setPrevWorld(const mat4& _prevWorld)     { m_prevWorld = _prevWorld; }
	int          getSelectedLOD() const                   { return m_selectedLOD; }
	void         setLODOverride(int _lod)                 { m_lodOverride = _lod; }
	void         setSubmeshOverride(int _submesh)         { m_subMeshOverride = _submesh; }

protected:

	bool         initImpl() override;
	bool         postInitImpl() override;
	void         shutdownImpl() override;
	bool         editImpl() override;
	bool         editFlag(const char* _name, Flag _flag);
	bool         serializeImpl(Serializer& _serializer_) override;
	bool         isStatic() override { return true; }

	Flags                                  m_flags;
	vec4                                   m_colorAlpha      = vec4(1.0f);
	mat4                                   m_world           = identity;
	mat4                                   m_prevWorld       = identity;
	DrawMesh*                              m_mesh            = nullptr;
	PathStr                                m_meshPath        = "";
	int                                    m_lodOverride     = -1;
	int                                    m_subMeshOverride = -1;
	eastl::fixed_vector<BasicMaterial*, 1> m_materials;      // per submesh
	eastl::fixed_vector<PathStr, 1>        m_materialPaths;  //     "
	eastl::fixed_vector<mat4, 1>           m_pose;
	eastl::fixed_vector<mat4, 1>           m_prevPose;

	// \todo Store this in the renderer.
	int m_selectedLOD = 0;

	friend class BasicRenderer;
	friend class RaytracingRenderer;
};

} // namespace frm
