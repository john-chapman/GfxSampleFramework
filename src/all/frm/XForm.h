#pragma once
#ifndef frm_XForm_h
#define frm_XForm_h

#include <frm/def.h>
#include <frm/math.h>

#include <frm/Input.h>
#include <frm/Scene.h>

#include <apt/StringHash.h>
#include <apt/Factory.h>

#include <vector>

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
	typedef void (OnComplete)(XForm* _xform_);
	struct Callback
	{
		OnComplete*     m_callback;
		const char*     m_name;
		apt::StringHash m_nameHash;
		
		Callback(const char* _name, OnComplete* _callback);
	};
	static int             GetCallbackCount();
	static const Callback* GetCallback(int _i);
	static const Callback* FindCallback(apt::StringHash _nameHash);
	static const Callback* FindCallback(OnComplete* _callback);
	static bool            SerializeCallback(const char* _name, OnComplete*& _callback, apt::JsonSerializer& _serializer_);

	/// Reset initial state.
	virtual void reset() {}
	static  void Reset(XForm* _xform_)         { _xform_->reset(); }

	/// Initial state + current state.
	virtual void relativeReset() {}
	static  void RelativeReset(XForm* _xform_) { _xform_->relativeReset(); }

	/// Reverse operation.
	virtual void reverse() {}
	static  void Reverse(XForm* _xform_)       { _xform_->reverse(); }

	const char*  getName() const               { return getClassRef()->getName(); }	
	Node*        getNode() const               { return m_node; }
	void         setNode(Node* _node)          { m_node = _node; }

	virtual void apply(float _dt) = 0;	
	virtual void edit() = 0;
	virtual bool serialize(apt::JsonSerializer& _serializer_) = 0;

protected:
	static std::vector<const Callback*> s_callbackRegistry;

	XForm(): m_node(nullptr)      {}

	Node* m_node;

}; // class XForm

#define XFORM_REGISTER_CALLBACK(_callback) \
	static XForm::Callback APT_UNIQUE_NAME(XForm_Callback_)(#_callback, _callback);

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
	virtual bool serialize(apt::JsonSerializer& _serializer_) override;
	
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
	virtual bool serialize(apt::JsonSerializer& _serializer_) override;
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
	virtual bool serialize(apt::JsonSerializer& _serializer_) override;
};

////////////////////////////////////////////////////////////////////////////////
/// \class XForm_Spin
/// Constant rotation at m_rate around m_axis.
////////////////////////////////////////////////////////////////////////////////
struct XForm_Spin: public XForm
{
	vec3  m_axis;
	float m_rate;      // radians/s
	float m_rotation;

	XForm_Spin();
	virtual ~XForm_Spin();

	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::JsonSerializer& _serializer_) override;
};

////////////////////////////////////////////////////////////////////////////////
/// \class XForm_PositionTarget
/// Translate between m_start -> m_end over m_duration seconds.
////////////////////////////////////////////////////////////////////////////////
struct XForm_PositionTarget: public XForm
{
	vec3  m_start;
	vec3  m_end;
	vec3  m_currentPosition;
	float m_duration;
	float m_currentTime;

	OnComplete* m_onComplete;

	XForm_PositionTarget();
	virtual ~XForm_PositionTarget();

	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::JsonSerializer& _serializer_) override;

	virtual void reset() override;
	virtual void relativeReset() override;
	virtual void reverse() override;
};

} // namespace frm

#endif // frm_XForm_h
