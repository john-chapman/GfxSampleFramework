#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/BitFlags.h>
#include <frm/vr/VRInput.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// VRController
// Manages user locomotion.
// There are a fixed number of primary 'actions' (turn, move, etc.), each with
// a group of settings which determine behavior/input mapping + a current state.
//
// \todo
// - Suppress motion blur on the VR context when snapping. Note that rotation
//   incurs movement in the user position (to rotate around the head), may need
//   to account for that.
////////////////////////////////////////////////////////////////////////////////
class VRController
{
public:
	struct Transition
	{
		float in;
		float out;
	};
	
	enum MoveMode_
	{
		MoveMode_None,        // Disable traversal.
		MoveMode_Snap,        // Teleport instantly to the target.
		MoveMode_Shift,       // Animate position to target.
		MoveMode_Continuous,  // Smooth traversal.

		MoveMode_Count
	};
	typedef int MoveMode;

	struct MoveSettings
	{
		MoveMode       mode                = MoveMode_Shift;
		VRInput::Axis  input               = VRInput::Axis_LThumbStickY;
		Transition     snapTransition      = { 0.f, 0.f };
		Transition     shiftTransition     = { 0.05f, 0.05f };
		float          snapMaxDistance     = 10.f;
		float          continuousMaxSpeed  = 5.f;
	};

	struct MoveState
	{
		float transition          = 1.f;
		vec3  startPosition       = vec3(0.f);
		vec3  targetPosition      = vec3(0.f);
		bool  targetPositionValid = false;
		vec3  targetNormal        = vec3(0.f);
	};


	enum TurnMode_
	{
		TurnMode_None,       // Disable turning.
		TurnMode_Snap,       // Rotate in fixed increments.
		TurnMode_Continuous, // Smooth rotation.

		TurnMode_Count
	};
	typedef int TurnMode;
	
	struct TurnSettings
	{
		TurnMode             mode           = TurnMode_Snap;
		VRInput::Axis        input          = VRInput::Axis_LThumbStickX;
		float                snapAngle      = 20.f;
		Transition           snapTransition = { 0.0f, 0.1f };
		float                continuousRate = 60.f;
	};

	struct TurnState
	{
		float startAngle  = 0.f;
		float targetAngle = 0.f;
		float transition  = 1.f;
	};


	virtual void update(float _dt, VRContext* _ctx);
	virtual bool edit();
	virtual bool serialize(Serializer& _serializer_);
	virtual void draw(float _dt, VRContext* _ctx) const;

	const vec3&  getPosition() const    { return m_position; }
	float        getOrientation() const { return m_orientation; }
	const mat4&  getTransform() const   { return m_transform; }

protected:

	enum class Action
	{
		Turn,
		Move,

		_Count,
		_Default = 0,
		None     = _Count
	};
	
	BitFlags<Action> m_actionState;
	MoveSettings     m_moveSettings;
	MoveState        m_moveState;
	TurnSettings     m_turnSettings;
	TurnState        m_turnState;

	vec3             m_position               = vec3(0.f);
	float            m_orientation            = 0.f;
	mat4             m_transform              = identity;
	Action           m_currentTransitionType  = Action::None;
	Transition       m_currentTransition;
	float            m_transitionState        = 0.f;

	void updateTransition(float _dt, VRContext* _ctx);
	void updateInput(float _dt, VRContext* _ctx);
	void startTransition(Action _action);
	void updatePositionOnTurn(VRContext* _ctx, float _preRotation, float _postRotation);
};

} // namespace frm
