#pragma once

#include <frm/core/world/components/Component.h>

#include <EASTL/span.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// ImageLightComponent
// Image-based light. Supports cubemap and rectilinear projected source images.
//
// \todo
// - Store all cubemaps in a single global array texture + use world space
//   extents and bounds for parallax correction + filtering.
// - BC6H compression + caching.
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(ImageLightComponent)
{
public:

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);
	static eastl::span<ImageLightComponent*> GetActiveComponents();

	float  getBrightness() const                 { return m_brightness; }
	void   getBrightness(float _brightness)      { m_brightness = _brightness; }
	bool   getIsBackground() const               { return m_isBackground; }
	void   setIsBackground(bool _isBackground)   { m_isBackground = _isBackground; }

protected:

	float    m_brightness   = 1.0f;
	bool     m_isBackground = false;  // If true, use to fill the background of the scene buffer.
	Texture* m_texture      = nullptr;
	PathStr  m_texturePath  = "";

	bool initImpl() override;
	void shutdownImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;

	// Call during init() or whenever the texture path changes.
	bool loadAndFilter();

	friend class BasicRenderer;
};

} // namespace frm