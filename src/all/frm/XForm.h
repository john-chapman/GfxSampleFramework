#pragma once
#ifndef frm_XForm_h
#define frm_XForm_h

#include <frm/def.h>
#include <frm/math.h>

#include <frm/Input.h>
#include <frm/Scene.h>

#include <apt/Factory.h>

namespace frm
{

class Node;

////////////////////////////////////////////////////////////////////////////////
/// \class XForm
/// Base class/factory for XForms.
////////////////////////////////////////////////////////////////////////////////
class XForm: public apt::Factory<XForm>
{
public:
	enum CompleteBehavior
	{
		kDestroy,
		kRepeat,
		kReverse
	};
	
	virtual void apply(Node* _node_, float _dt) = 0;	
	virtual void edit()                         = 0;
	
	const char* getName() const { return getClassRef()->getName(); }
	
}; // class XForm


////////////////////////////////////////////////////////////////////////////////
/// \class PositionOrientationScaleXForm
////////////////////////////////////////////////////////////////////////////////
struct PositionOrientationScaleXForm: public XForm
{
	vec3  m_position;
	quat  m_orientation;
	vec3  m_scale;

	PositionOrientationScaleXForm();
	virtual ~PositionOrientationScaleXForm() {}

	virtual void apply(Node* _node_, float _dt) override;
	virtual void edit() override;
};

////////////////////////////////////////////////////////////////////////////////
/// \class FreeCameraXForm
/// Apply keyboard/gamepad input.
/// Mouse/Keyboard:
///   - W/A/S/D = forward/left/backward/right
///   - Q/E = down/up
///   - Left Shift = accelerate
///   - Mouse = look
///
/// Gamepad:
///    - Left Stick = move
///    - Left/Right shoulder buttons = down/up
///    - Right Trigger = accelerate
///    - Right Stick = look
////////////////////////////////////////////////////////////////////////////////
struct FreeCameraXForm: public XForm
{
	vec3  m_position;
	vec3  m_velocity;
	float m_speed;
	float m_maxSpeed;
	float m_maxSpeedMul;        //< Multiplies m_speed for speed 'boost'.
	float m_accelTime;          //< Acceleration ramp length in seconds.
	float m_accelCount;         //< Current ramp position in [0,m_accelTime].

	quat  m_orientation;
	vec3  m_pitchYawRoll;       //< Angular velocit in rads/sec.
	float m_rotationInputMul;   //< Scale rotation inputs (should be relative to fov/screen size).
	float m_rotationDamp;       //< Adhoc damping factor.

	
	FreeCameraXForm();
	virtual ~FreeCameraXForm() {}

	virtual void apply(Node* _node_, float _dt) override;
	virtual void edit() override;
};

////////////////////////////////////////////////////////////////////////////////
/// \class LookAtXForm
/// Overrides the world matrix with a 'look at' matrix.
////////////////////////////////////////////////////////////////////////////////
struct LookAtXForm: public XForm
{
	const Node* m_target; //< Node to look at (can be 0).
	vec3 m_offset;        //< Offset from target, or world space if target is 0.

	LookAtXForm();
	virtual ~LookAtXForm() {}

	virtual void apply(Node* _node_, float _dt) override;
	virtual void edit() override;
}; 

} // namespace frm

#endif // frm_XForm_h
