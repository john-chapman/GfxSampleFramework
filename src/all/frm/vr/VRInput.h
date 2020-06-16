#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/Input.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// VRInput
// - Both hands are represented by a single controller since they are typically
//   polled together as a single state.
// - 3d Hand poses are managed separately by the VR context.
//
// \todo
// - See Input.h/Device
// - Given that the capabilities of VR hand controllers vary significantly, 
//   multiplexing the device interface via enums may not work well.
////////////////////////////////////////////////////////////////////////////////
class VRInput: public Device
{
public:

	enum Button
	{
		Button_Unmapped = 0,

	// Oculus touch
	// 'Touch_' buttons are the capacitive sensors on the controller.
	// 'Pose_' buttons are derived from the touch state.		

		// Left hand.
		Button_LMenu,
		Button_X,        Touch_X,
		Button_Y,        Touch_Y,
		Button_LThumb,   Touch_LThumb,
		                 Touch_LThumbrest,
		Button_LTrigger, Touch_LTrigger,
		Button_LGrip,
		Pose_LIndexPointing,
		Pose_LThumbUp,
		

		// Right hand.
		Button_RMenu, // Oculus button

		Button_A,        Touch_A,
		Button_B,        Touch_B,
		Button_RThumb,   Touch_RThumb,
		                 Touch_RThumbrest,
		Button_RTrigger, Touch_RTrigger,
		Button_RGrip,
		Pose_RIndexPointing,
		Pose_RThumbUp,
		
		Button_Count,

		// Ranges.
		Button_Left_Begin  = Button_LMenu,   Button_Left_End  = Button_RMenu,
		Button_Right_Begin = Button_RMenu,   Button_Right_End = Button_Count,
	};

	enum Axis
	{
		Axis_Unmapped = 0,

	// Oculus touch

		// Left hand.
		Axis_LThumbStickX, Axis_LThumbStickY,
		Axis_LTrigger,
		Axis_LGrip,

		// Right hand.
		Axis_RThumbStickX, Axis_RThumbStickY,
		Axis_RTrigger,
		Axis_RGrip,

		Axis_Count,

		// Thumbstick XY may be accessed simultaneously (see getThumbStickXY()).
		Axis_LThumbStick = Axis_LThumbStickX,
		Axis_RThumbStick = Axis_RThumbStickX,

		// Ranges.
		Axis_Left_Begin  = Axis_LThumbStick,   Axis_Left_End  = Axis_RThumbStickX,
		Axis_Right_Begin = Axis_LGrip,         Axis_Right_end = Axis_Count,
	};

	// Return both thumbstick axes as a single vec2.
	const vec2&     getThumbStickXY(Axis _axis) const   { FRM_ASSERT(_axis == Axis_LThumbStick || _axis == Axis_RThumbStick); return *((vec2*)&m_axisStates[_axis]); }

	// Return the hand index for a given axis/button.
	int             getButtonHand(Button _button) const { return (_button >= Button_Left_Begin && _button < Button_Left_End) ? 0 : 1; }
	int             getAxisHand(Axis _axis) const       { return (_axis   >= Axis_Left_Begin   && _axis   < Axis_Left_End)   ? 0 : 1; }

protected:

	VRInput()
		: Device(Button_Count, Axis_Count)
	{
	}

	friend class VRContext;
};

} // namespace frm
