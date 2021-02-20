#pragma once

#include "Component.h"

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// CameraComponent
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(CameraComponent)
{
public: 	

	// Get/set the current draw camera.
	static CameraComponent* GetDrawCamera()                                     { return s_drawCamera[0]; }
	static void             SetDrawCamera(CameraComponent* _cameraComponent);

	// Get/set the current cull camera.
	static CameraComponent* GetCullCamera()                                     { return s_cullCamera[0]; }
	static void             SetCullCamera(CameraComponent* _cameraComponent);

	// Update active components.
	static void             Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);


	void                    lookAt(const vec3& _from, const vec3& _to, const vec3& _up = vec3(0.0f, 1.0f, 0.0f));

	Camera&                 getCamera()                                         { return m_camera; }

private:

	static CameraComponent* s_drawCamera[2]; // 0 = current, 1 = previous
	static CameraComponent* s_cullCamera[2]; // 0 = current, 1 = previous

	Camera m_camera;

	void draw() const;
	bool initImpl() override;
	void shutdownImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;

	bool isStatic() override { return true; }
};


} // namespace frm