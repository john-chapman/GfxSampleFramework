#include "VRController.h"

#include <frm/core/interpolation.h>
#include <frm/core/Serializer.h>
#include <frm/vr/VRContext.h>
#include <frm/vr/VRInput.h>

#if FRM_MODULE_PHYSICS
	#include <frm/physics/Physics.h>
#endif

#include <im3d/im3d.h>
#include <imgui/imgui.h>

static float SmoothStep(float _a, float _b, float _x)
{
	float t = frm::Saturate((_x - _a) / (_b - _a));
    return t * t * (3.f - 2.f * t);
}

namespace frm {

// PUBLIC

void VRController::update(float _dt, VRContext* _ctx)
{
	if (m_currentTransitionType != Action::None)
	{
		updateTransition(_dt, _ctx);
	}
	else
	{
		updateInput(_dt, _ctx);
	}
	
	const VRContext::TrackedData& trackedData = _ctx->getTrackedData();
	m_transform = TransformationMatrix(m_position, RotationQuaternion(vec3(0.f, 1.f, 0.f), Radians(m_orientation)));
	{
		#if 0
			Im3d::PushDrawState();
			Im3d::PushMatrix(m_transform);
				Im3d::SetColor(Im3d::Color_Magenta);
				Im3d::SetAlpha(0.3f);
				Im3d::DrawCircleFilled(vec3(0.0f), vec3(0.0f, 1.0f, 0.0f), 0.2f, 32);
				Im3d::SetAlpha(1.0f);
				Im3d::SetSize(4.0f);
				Im3d::DrawCircle(vec3(0.0f), vec3(0.0f, 1.0f, 0.0f), 0.2f, 32);
				Im3d::SetSize(16.0f);
				Im3d::DrawArrow(vec3(0.0f, 0.0f, 0.5f), vec3(0.0f, 0.0f, 1.0f));
			Im3d::PopMatrix();

			vec3 headPosition = _ctx->getTrackedData().headPose.pose[3].xyz() * vec3(1.f, 0.f, 1.f);
			Im3d::PushMatrix();
				Im3d::Translate(headPosition);
				Im3d::SetColor(Im3d::Color_Magenta);
				Im3d::SetAlpha(0.3f);
				Im3d::DrawCircleFilled(vec3(0.0f), vec3(0.0f, 1.0f, 0.0f), 0.1f, 32);
				Im3d::SetAlpha(1.0f);
				Im3d::SetSize(4.0f);
				Im3d::DrawCircle(vec3(0.0f), vec3(0.0f, 1.0f, 0.0f), 0.1f, 32);
			Im3d::PopMatrix();
			Im3d::PopDrawState();
		#endif
	}
}

bool VRController::edit()
{
	bool ret = false;

	return ret;
}

bool VRController::serialize(Serializer& _serializer_)
{
	bool ret = true;

	return ret;
}

void VRController::draw(float _dt, VRContext* _ctx) const
{
	const VRContext::TrackedData& trackedData = _ctx->getTrackedData();

	static float beadT = 0.0f;
	beadT = Fract(beadT + _dt * 2.0f);

	Im3d::PushDrawState();
		if (m_actionState.get(Action::Move) && (m_moveSettings.mode == MoveMode_Snap || m_moveSettings.mode == MoveMode_Shift))
		{
			vec3  lineStart  = trackedData.handPoses[_ctx->getInputDevice().getAxisHand(m_moveSettings.input)].pose[3].xyz();
			vec3  lineEnd    = m_moveState.targetPosition;
			vec3  lineDir    = lineEnd - lineStart;
			float lineLen    = Length(lineDir);
			      lineDir    = lineDir / lineLen;
	
			const Im3d::Color color = m_moveState.targetPositionValid ? Im3d::Color_Gold : Im3d::Color_Orange;
			const float alpha = m_moveState.targetPositionValid ? 1.0f : 0.25f;
			Im3d::SetSize(6.0f);
			Im3d::SetColor(color);
			Im3d::BeginLineStrip();
				Im3d::SetAlpha(0.0f);
				Im3d::Vertex(lineStart + lineDir * lineLen * 0.05f);
				Im3d::SetAlpha(1.0f * alpha);
				Im3d::Vertex(lineStart + lineDir * lineLen * 0.5f);
				Im3d::SetSize(1.0f);
				Im3d::SetAlpha(0.0f);
				Im3d::Vertex(lineEnd);
			Im3d::End();
	
			if (m_moveState.targetPositionValid)
			{
				Im3d::SetAlpha(0.3f * alpha);
				Im3d::DrawCircleFilled(m_moveState.targetPosition, m_moveState.targetNormal, 0.2f, 64);
				Im3d::SetAlpha(1.0f * alpha);
				Im3d::SetSize(2.0f);
				Im3d::DrawCircle(m_moveState.targetPosition, m_moveState.targetNormal, 0.2f, 64);
	
				float beadAlpha = SmoothStep(0.0f, 0.3f, beadT) * (1.f - SmoothStep(0.5f, 1.f, beadT));
				Im3d::SetAlpha(1.f * alpha * beadAlpha);
				Im3d::DrawPoint(lineStart + lineDir * lineLen * beadT, 10.0f * beadAlpha, Im3d::Color_Yellow);
			}
		}			
	Im3d::PopDrawState();
}

// PROTECTED

void VRController::updateTransition(float _dt, VRContext* _ctx)
{
	const float rate = 1.f / ((m_transitionState >= 0.f) ? m_currentTransition.out : m_currentTransition.in);
	m_transitionState = Min(1.0f, m_transitionState + _dt * rate);

	switch (m_currentTransitionType)
	{
		default:
		case Action::Move:
		{
			switch (m_moveSettings.mode)
			{
				default:
				case MoveMode_Continuous:
					break;
				case MoveMode_Snap:
					if (m_transitionState >= 0.f)
					{
						m_position = m_moveState.targetPosition;
					}
					break;
				case MoveMode_Shift:
					m_position = lerp(m_moveState.startPosition, m_moveState.targetPosition, m_transitionState * 0.5f + 0.5f);
					break;
			};
			break;
		}
		case Action::Turn:
		{
			switch (m_turnSettings.mode)
			{
				default:
				case TurnMode_Continuous:
					break;
				case TurnMode_Snap:
					if (m_transitionState >= 0.f)
					{
						m_orientation = m_turnState.targetAngle;
					}
					break;
			};
			break;
		}
	};

	if (m_transitionState >= 1.0f)
	{
		m_currentTransitionType = Action::None;
	}
}

void VRController::updateInput(float _dt, VRContext* _ctx)
{
	const VRContext::TrackedData& trackedData = _ctx->getTrackedData();
	VRInput& input = _ctx->getInputDevice();
	const float kTurnAxisDeadzone = 0.6f; // \todo
	const float kMoveAxisDeadzone = (m_moveSettings.mode == MoveMode_Continuous) ? 0.2f : 0.95f; // \todo

	// Move.
	float moveInput = input.getAxisState(m_moveSettings.input);
	if (m_moveSettings.mode != MoveMode_None && Abs(moveInput) > kMoveAxisDeadzone)
	{
		m_actionState.set(Action::Move, true);

		switch (m_moveSettings.mode)
		{
			default:
			case MoveMode_Snap:
			case MoveMode_Shift:
			{
				const VRContext::Hand handIndex = input.getAxisHand(m_moveSettings.input);
				const vec3 handPosition  = trackedData.handPoses[handIndex].getPosition();
				const vec3 handDirection = trackedData.handPoses[handIndex].getForwardVector();

				#if FRM_MODULE_PHYSICS
				{
					const mat4& handPose = trackedData.handPoses[input.getAxisHand(m_moveSettings.input)].pose;

					Physics::RayCastIn  rayIn(handPosition + handDirection * 0.15f, handDirection);
					Physics::RayCastOut rayOut;
					if (Physics::RayCast(rayIn, rayOut))
					{
						m_moveState.targetPosition = rayOut.position;
						m_moveState.targetNormal   = rayOut.normal;

						// \todo validate that the user can stand in the target position
						if (rayOut.normal.y < 0.5f || rayOut.component->getFlag(Physics::Flag::Static)) // sloping or dynamic surface
						{
							m_moveState.targetPositionValid = false;
						}
						else
						{
							m_moveState.targetPositionValid = true;
						}
					}
				}
				#else
					// plane at 0?
				#endif
				break;
			}
			case MoveMode_Continuous:
			{
			// \todo This is too basic to work well. 
			// - Needs to use a full physics capsule controller where available (https://documentation.help/NVIDIA-PhysX-SDK-Guide/CharacterControllers.html).
			// - Needs to use both axes of the thumbstick, which affects how snap turns are mapped.
				const float speed  = _dt * m_moveSettings.continuousMaxSpeed * smooth(0.0f, 1.0f, Abs(moveInput)) * Sign(moveInput);
				const vec2 heading = -Normalize(vec2(trackedData.headPose.pose[2].x, trackedData.headPose.pose[2].z));
				m_position.x += speed * heading.x;
				m_position.z += speed * heading.y;

				// After moving, raycast down to adjust vertical world position.
				#if FRM_MODULE_PHYSICS
				{
					// \todo m_position is actually the origin of the current playspace, any world-space raycasts must happen from the actual user position.
					vec3 headPosition = trackedData.headPose.getPosition();

					Physics::RayCastIn  rayIn(headPosition, vec3(0.0f, -1.0f, 0.0f));
					Physics::RayCastOut rayOut;
					if (Physics::RayCast(rayIn, rayOut, Physics::RayCastFlag::Position))
					{
						m_position.y = rayOut.position.y;
					}
				}
				#endif
				break;
			}
		};
	}
	else
	{
		if (m_actionState.get(Action::Move) && m_moveSettings.mode != MoveMode_Continuous)
		{
			if (m_moveState.targetPositionValid)
			{
				vec3 headOffset = trackedData.headOffset * vec3(1.f, 0.f, 1.f);
				m_moveState.targetPosition -= qrot(RotationQuaternion(vec3(0.f, 1.f, 0.f), Radians(m_orientation)), headOffset); 
				startTransition(Action::Move);
			}
		}
		m_actionState.set(Action::Move, false);
	}

	// Turn.
	float turnInput = -input.getAxisState(m_turnSettings.input); 
	if (m_turnSettings.mode != TurnMode_None && Abs(turnInput) > kTurnAxisDeadzone)
	{
		if (!m_actionState.get(Action::Turn) && m_turnSettings.mode == TurnMode_Snap)
		{
			m_turnState.targetAngle = m_orientation + m_turnSettings.snapAngle * Sign(turnInput);
			updatePositionOnTurn(_ctx, m_orientation, m_turnState.targetAngle);

			startTransition(Action::Turn);
		}
		
		m_actionState.set(Action::Turn, true);

		if (m_turnSettings.mode == TurnMode_Continuous)
		{
			float preRotation = m_orientation;
			m_orientation += Sign(turnInput) * m_turnSettings.continuousRate * _dt;
			updatePositionOnTurn(_ctx, preRotation, m_orientation);
		}
	}
	else
	{
		m_actionState.set(Action::Turn, false);
	}
}

void VRController::startTransition(Action _action)
{
	FRM_ASSERT(m_currentTransitionType == Action::None);

	m_currentTransitionType = _action;
	m_transitionState = -1.0f;

	switch (_action)
	{
		default:
		case Action::Move:
		{
			m_moveState.startPosition = m_position;
			switch (m_moveSettings.mode)
			{
				default:
				case MoveMode_Continuous:
					break;
				case MoveMode_Snap:
					m_currentTransition = m_moveSettings.snapTransition;
					break;
				case MoveMode_Shift:
					m_currentTransition = m_moveSettings.shiftTransition;
					break;
			};
			break;
		}
		case Action::Turn:
		{
			m_turnState.startAngle = m_orientation;
			switch (m_turnSettings.mode)
			{
				default:
				case TurnMode_Continuous:
					break;
				case TurnMode_Snap:
					m_currentTransition = m_turnSettings.snapTransition;
					break;
			};
			break;
		}
	};
}

void VRController::updatePositionOnTurn(VRContext* _ctx, float _preRotation, float _postRotation)
{
	const vec3& headOffset = _ctx->getTrackedData().headOffset;
	vec3 headOffset1 = qrot(RotationQuaternion(vec3(0.f, 1.f, 0.f), Radians(_preRotation)),  headOffset);
	vec3 headOffset2 = qrot(RotationQuaternion(vec3(0.f, 1.f, 0.f), Radians(_postRotation)), headOffset);
	m_position += headOffset1 - headOffset2;
}

} // namespace frm
