#pragma once

#include <frm/core/def.h>
#include <frm/core/math.h>
#include <frm/core/Input.h>
#include <frm/core/Scene.h>

#include <apt/StringHash.h>
#include <apt/Factory.h>

#include <EASTL/vector.h>

namespace frm {

class Node;

////////////////////////////////////////////////////////////////////////////////
// XForm
// Base class/factory for XForms.
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
	static bool            SerializeCallback(apt::Serializer& _serializer_, OnComplete*& _callback, const char* _name);

	// Reset initial state.
	virtual void reset() {}
	static  void Reset(XForm* _xform_)         { _xform_->reset(); }

	// Initial state + current state.
	virtual void relativeReset() {}
	static  void RelativeReset(XForm* _xform_) { _xform_->relativeReset(); }

	// Reverse operation.
	virtual void reverse() {}
	static  void Reverse(XForm* _xform_)       { _xform_->reverse(); }

	const char*  getName() const               { return getClassRef()->getName(); }	
	Node*        getNode() const               { return m_node; }
	void         setNode(Node* _node)          { m_node = _node; }

	virtual void apply(float _dt) = 0;	
	virtual void edit() = 0;
	virtual bool serialize(apt::Serializer& _serializer_) = 0;
	friend bool Serialize(apt::Serializer& _serializer_, XForm& _xform_)
	{
		return _xform_.serialize(_serializer_);
	}

protected:
	static eastl::vector<const Callback*> s_callbackRegistry;

	XForm(): m_node(nullptr)      {}

	Node* m_node;

}; // class XForm

#define XFORM_REGISTER_CALLBACK(_callback) \
	static XForm::Callback APT_UNIQUE_NAME(XForm_Callback_)(#_callback, _callback);

////////////////////////////////////////////////////////////////////////////////
// XForm_PositionOrientationScale
////////////////////////////////////////////////////////////////////////////////
struct XForm_PositionOrientationScale: public XForm
{
	vec3  m_position      = vec3(0.0f);
	quat  m_orientation   = quat(0.0f, 0.0f, 0.0f, 1.0f);
	vec3  m_scale         = vec3(1.0f);
	
	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::Serializer& _serializer_) override;
	
};

////////////////////////////////////////////////////////////////////////////////
// XForm_FreeCamera
// Apply keyboard/gamepad input.
// Mouse/Keyboard:
//   - W/A/S/D = forward/left/backward/right
//   - Q/E = down/up
//   - Left Shift = accelerate
//   - Mouse + Mouse Right = look
//
// Gamepad:
//    - Left Stick = move
//    - Left/Right shoulder buttons = down/up
//    - Right Trigger = accelerate
//    - Right Stick = look
////////////////////////////////////////////////////////////////////////////////
struct XForm_FreeCamera: public XForm
{
	vec3  m_position           = vec3(0.0f);
	vec3  m_velocity           = vec3(0.0f);
	float m_speed              = 0.0f;
	float m_maxSpeed           = 10.0f;
	float m_maxSpeedMul        = 5.0f;                          // Multiplies m_speed for speed 'boost'.
	float m_accelTime          = 0.1f;                          // Acceleration ramp length in seconds.
	float m_accelCount         = 0.0f;                          // Current ramp position in [0,m_accelTime].

	quat  m_orientation        = quat(0.0f, 0.0f, 0.0f, 1.0f);
	vec3  m_pitchYawRoll       = vec3(0.0f);                    // Angular velocity in rads/sec.
	float m_rotationInputMul   = 0.1f;                          // Scale rotation inputs (should be relative to fov/screen size).
	float m_rotationDamp       = 0.0002f;                       // Adhoc damping factor.

	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::Serializer& _serializer_) override;
};

////////////////////////////////////////////////////////////////////////////////
// XForm_LookAt
// Overrides the world matrix with a 'look at' matrix.
////////////////////////////////////////////////////////////////////////////////
struct XForm_LookAt: public XForm
{
	Node*     m_target    = nullptr;          // Node to look at (can be 0).
	Node::Id  m_targetId  = Node::kInvalidId; // Required for serialization.
	vec3      m_offset    = vec3(0.0f);       // Offset from target, or world space if target is 0.

	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::Serializer& _serializer_) override;
};

////////////////////////////////////////////////////////////////////////////////
// XForm_Spin
// Constant rotation at m_rate around m_axis.
////////////////////////////////////////////////////////////////////////////////
struct XForm_Spin: public XForm
{
	vec3  m_axis       = vec3(0.0f, 0.0f, 1.0f);
	float m_rate       = 0.0f; // radians/s
	float m_rotation   = 0.0f;
	
	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::Serializer& _serializer_) override;
};

////////////////////////////////////////////////////////////////////////////////
// XForm_PositionTarget
// Translate between m_start -> m_end over m_duration seconds.
////////////////////////////////////////////////////////////////////////////////
struct XForm_PositionTarget: public XForm
{
	vec3  m_start             = vec3(0.0f);
	vec3  m_end               = vec3(0.0f);
	vec3  m_currentPosition   = vec3(0.0f);
	float m_duration          = 1.0f;
	float m_currentTime       = 0.0f;

	OnComplete* m_onComplete;
	
	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::Serializer& _serializer_) override;

	virtual void reset() override;
	virtual void relativeReset() override;
	virtual void reverse() override;
};

////////////////////////////////////////////////////////////////////////////////
// XForm_SplinePath
////////////////////////////////////////////////////////////////////////////////
struct XForm_SplinePath: public XForm
{
	SplinePath* m_path          = nullptr;
	int         m_pathHint      = 0;
	float       m_duration      = 1.0f;
	float       m_currentTime   = 0.0f;

	OnComplete* m_onComplete;

	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::Serializer& _serializer_) override;

	virtual void reset() override;
	virtual void reverse() override;
};

////////////////////////////////////////////////////////////////////////////////
// XForm_OrbitalPath
// Circular path oriented by azimuth/elevation angle.
////////////////////////////////////////////////////////////////////////////////
struct XForm_OrbitalPath: public XForm
{
	float m_azimuth         = 0.0f;
	float m_elevation       = 90.0f;
	float m_theta           = 0.0f;      // distance along path
	float m_radius          = 1.0f;
	float m_speed           = 0.0f;
	vec3  m_direction       = vec3(0.0f);
	vec3  m_normal          = vec3(0.0f);
	vec4  m_displayColor    = vec4(1.0f, 1.0f, 0.0f, 1.0f);

	virtual void apply(float _dt) override;
	virtual void edit() override;
	virtual bool serialize(apt::Serializer& _serializer_) override;

	virtual void reset() override;
};

} // namespace frm
