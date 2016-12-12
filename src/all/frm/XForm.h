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
	virtual void apply(float _dt) = 0;	
	virtual void edit()           = 0;
	
	const char* getName() const   { return getClassRef()->getName(); }
	
	Node* getNode() const         { return m_node; }
	void  setNode(Node* _node)    { m_node = _node; }

protected:
	XForm(): m_node(nullptr)      {}

	Node* m_node;

}; // class XForm


////////////////////////////////////////////////////////////////////////////////
/// \class BasicAnimationXForm
////////////////////////////////////////////////////////////////////////////////
struct BasicAnimationXForm: public XForm
{
	typedef void (OnComplete)(BasicAnimationXForm* _xform_);
	OnComplete* m_onComplete;
	bool        m_active;

	BasicAnimationXForm()
		: m_onComplete(nullptr)
		, m_active(true)
	{
	}

	/// Reset initial state.
	virtual void reset() {}
	static  void Reset(BasicAnimationXForm* _xform_)         { _xform_->reset(); }

	/// Initial state + current state.
	virtual void relativeReset() {}
	static  void RelativeReset(BasicAnimationXForm* _xform_) { _xform_->relativeReset(); }

	/// Reverse operation.
	virtual void reverse() {}
	static  void Reverse(BasicAnimationXForm* _xform_)       { _xform_->reverse(); }
};

////////////////////////////////////////////////////////////////////////////////
/// \class XForm_PositionOrientationScale
////////////////////////////////////////////////////////////////////////////////
struct XForm_PositionOrientationScale: public XForm
{
	vec3  m_position;
	quat  m_orientation;
	vec3  m_scale;

	XForm_PositionOrientationScale();
	virtual ~XForm_PositionOrientationScale() {}

	virtual void apply(float _dt) override;
	virtual void edit() override;
};

////////////////////////////////////////////////////////////////////////////////
/// \class XForm_FreeCamera
/// Apply keyboard/gamepad input.
/// Mouse/Keyboard:
///   - W/A/S/D = forward/left/backward/right
///   - Q/E = down/up
///   - Left Shift = accelerate
///   - Mouse + Mouse Right = look
///
/// Gamepad:
///    - Left Stick = move
///    - Left/Right shoulder buttons = down/up
///    - Right Trigger = accelerate
///    - Right Stick = look
////////////////////////////////////////////////////////////////////////////////
struct XForm_FreeCamera: public XForm
{
	vec3  m_position;
	vec3  m_velocity;
	float m_speed;
	float m_maxSpeed;
	float m_maxSpeedMul;        //< Multiplies m_speed for speed 'boost'.
	float m_accelTime;          //< Acceleration ramp length in seconds.
	float m_accelCount;         //< Current ramp position in [0,m_accelTime].

	quat  m_orientation;
	vec3  m_pitchYawRoll;       //< Angular velocity in rads/sec.
	float m_rotationInputMul;   //< Scale rotation inputs (should be relative to fov/screen size).
	float m_rotationDamp;       //< Adhoc damping factor.

	
	XForm_FreeCamera();
	virtual ~XForm_FreeCamera() {}

	virtual void apply(float _dt) override;
	virtual void edit() override;
};

////////////////////////////////////////////////////////////////////////////////
/// \class XForm_LookAt
/// Overrides the world matrix with a 'look at' matrix.
////////////////////////////////////////////////////////////////////////////////
struct XForm_LookAt: public XForm
{
	const Node* m_target; //< Node to look at (can be 0).
	vec3 m_offset;        //< Offset from target, or world space if target is 0.

	XForm_LookAt();
	virtual ~XForm_LookAt() {}

	virtual void apply(float _dt) override;
	virtual void edit() override;
};

////////////////////////////////////////////////////////////////////////////////
/// \class XForm_Spin
/// Constant rotation at m_rate around m_axis.
////////////////////////////////////////////////////////////////////////////////
struct XForm_Spin: public XForm
{
	vec3  m_axis;      //< Axis of rotation.
	float m_rate;      //< Rate of rotation, radians/s.
	float m_rotation;  //< Current rotation.

	XForm_Spin();
	virtual ~XForm_Spin();

	virtual void apply(float _dt) override;
	virtual void edit() override;
};

} // namespace frm

#endif // frm_XForm_h
