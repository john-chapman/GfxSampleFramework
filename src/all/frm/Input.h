#pragma once
#ifndef frm_Input_h
#define frm_Input_h

#include <frm/def.h>

#include <apt/static_initializer.h>

namespace frm {

class Window;

////////////////////////////////////////////////////////////////////////////////
// Device
// Base class for input devices. Logical devices consist of buttons and axes.
// Buttons: button state is a char whose high order bit indicates the button
//    state (0 = up, 1 = down). The remaining bits contain the number of 
//    complete button presses between the previous two polling frames.
// Axes: axis state is a float whose range is device dependent. In most cases
//    (for mouse or pointer devices and gamepad axes) axis state is relative,
//    i.e. is a delta from the previous state.
////////////////////////////////////////////////////////////////////////////////
class Device
{
	friend class ProxyDevice;
public:
	// Whether a physical device is connected.
	bool isConnected() const                    { return m_isConnected; }

	// Whether _button was pressed between the previous two calls to pollState().
	bool wasPressed(int _button) const          { return (getButtonState(_button) & 0x7f) != 0; }
	// Whether _button is in the 'down' state (state != 0).
	bool isDown(int _button) const              { return (getButtonState(_button) & 0x80) != 0; }
	// Whether _button is in the 'up' state (state == 0).
	bool isUp(int _button) const                { return !isDown(_button); }
	// # of button presses between the previous two calls to pollState().
	char getPressCount(int _button) const       { return getButtonState(_button) & 0x7f; }

	// Raw button state. High order bit contains the button state, low bits contain the press count.
	char getButtonState(int _button) const      { APT_ASSERT(_button < m_buttonCount); return m_buttonStates ? m_buttonStates[_button] : 0; }
	// Raw axis state.
	float getAxisState(int _axis) const         { APT_ASSERT(_axis < m_axisCount); return m_axisStates ? m_axisStates[_axis] : 0.0f; }

	int getButtonCount() const                  { return m_buttonCount; }
	int getAxisCount() const                    { return m_axisCount; }

protected:
	float     m_deltaTime;        // Seconds since last poll.
	bool      m_isConnected;      // Whether a physical device is connected.

	char*     m_buttonStates;     // Current button states.
	int       m_buttonCount;      // Size of button state buffer.

	float*    m_axisStates;       // Current axis states.
	int       m_axisCount;        // Size of axis state buffer.


	Device(int _buttonCount, int _axisCount);
	~Device();

	// Deriving classes should call Device::pollBegin() at the start of their polling implementation.
	void pollBegin();
	
	// Deriving classes should call Device::pollEnd() at the end of their polling implementation.
	void pollEnd();

	// Set button state (true = down), incremement the press count if the button is transitioning from down -> up.
	void setIncButton(int _button, bool _isDown);
	
	// Reset all states to default (zero the internal state buffers).
	void reset();
	
	void setButtonCount(int _count);
	void setAxisCount(int _count);

}; // class Device

////////////////////////////////////////////////////////////////////////////////
// Keyboard
// Default keyboard.
////////////////////////////////////////////////////////////////////////////////
class Keyboard: public Device
{
	friend class Input;
public:	
	enum Button
	{
		kUnmapped = 0, //< kUnmapped is a valid button with an undefined state (must have index 0).

	 // function keys
		kEscape, kPause, kPrintScreen, kCapsLock, kNumLock, kScrollLock, kInsert, kDelete, kClear,
		kLShift, kLCtrl, kLAlt, kRShift, kRCtrl, kRAlt,
		kF1, kF2, kF3, kF4, kF5, kF6, kF7, kF8, kF9, kF10, kF11, kF12,
		
	 // cursor control
		kSpace, kBackspace, kReturn, kTab,
		kPageUp, kPageDown, kHome, kEnd,
		kUp, kDown, kLeft, kRight,
		
	 // character keys
		kA, kB, kC, kD, kE, kF, kG, kH, kI, kJ, kK, kL, kM, kN, kO, kP, kQ, kR, kS, kT, kU, kV, kW, kX, kY, kZ,
		k1, k2, k3, k4, k5, k6, k7, k8, k9, k0,
		kPlus, kMinus, kPeriod, kComma,
		kMisc0, kMisc1, kMisc2, kMisc3, kMisc4, kMisc5, kMisc6, kMisc7, // layout-specific
		
	 // numpad
		kNumpad0, kNumpad1, kNumpad2, kNumpad3, kNumpad4, kNumpad5, kNumpad6, kNumpad7, kNumpad8, kNumpad9,
		kNumpadEnter, kNumpadPlus, kNumpadMinus, kNumpadMultiply, kNumpadDivide, kNumpadPeriod,

		kButtonCount
	};

	static const char* GetButtonName(Button _button) { return s_buttonNames[_button]; }

protected:
	Keyboard(): Device(kButtonCount, 0) {}

	static const char* s_buttonNames[kButtonCount];
	static void InitButtonNames();

}; // class Keyboard

////////////////////////////////////////////////////////////////////////////////
// Mouse
// Default mouse with left/middle/right buttons, x/y/wheel axes.
////////////////////////////////////////////////////////////////////////////////
class Mouse: public Device
{
	friend class Input;
public:
	
	enum Button
	{
		kLeft, kMiddle, kRight, kButtonCount
	};
	enum Axis
	{
		kX, kY, kWheel, kAxisCount
	};

	static const char* GetButtonName(Button _button) { return s_buttonNames[_button]; }

protected:
	Mouse(): Device(kButtonCount, kAxisCount) {}

	static const char* s_buttonNames[kButtonCount];
	static void InitButtonNames();
	
	/// Mouse zeroes axis states at the beginning of each polling frame.
	void pollBegin();

}; // class Mouse

////////////////////////////////////////////////////////////////////////////////
// Gamepad
// Default gamepad.
////////////////////////////////////////////////////////////////////////////////
class Gamepad: public Device
{
	friend class Input;
public:	
	enum Button
	{
		kUnmappedButton = 0,

	 // default/XBox
		kA, kB, kX, kY,
		kUp, kDown, kLeft, kRight,
		kLeft1, kLeft2, kLeft3, kRight1, kRight2, kRight3,
		kStart, kBack,

		kButtonCount,

	 // Playstation
		kCross = kA, kCircle = kB, kSquare = kX, kTriangle = kY,
		kSelect = kBack,
	};
	enum Axis
	{
		kUnmappedAxis = 0,

		kLeftStickX, kLeftStickY, kRightStickX, kRightStickY,
		kLeftTrigger, kRightTrigger,
		kPitch, kYaw, kRoll,
		kTouchpadX, kTouchpadY,

		kAxisCount
	};

	static const char* GetButtonName(Button _button) { return s_buttonNames[_button]; }

protected:
	float m_deadZone;

	Gamepad()
		: Device(kButtonCount, kAxisCount)
		, m_deadZone(0.2f)
	{
	}

	static const char* s_buttonNames[kButtonCount];
	static void InitButtonNames();

}; // class Gamepad

////////////////////////////////////////////////////////////////////////////////
// Input
// Manages supported input devices. Call PollAllDevices() once per frame to
// update all device states.
// \note Get*() functions will always return a valid Device (if _id is in the
//    correct range). The application should call Device::isConnected() to check
//    whether the logical device is backed by a physical device.
// \note The default id 0 is guaranteed always to be backed by a physical 
//    device if such a device is connected, i.e. if mouse 0 is disconnected
//    and another mouse is present, it will be reassigned to id 0.
////////////////////////////////////////////////////////////////////////////////
class Input
{
public:
	static const int kMaxKeyboardCount = 1;
	static const int kMaxMouseCount    = 1;
	static const int kMaxGamepadCount  = 4;

	static void PollAllDevices();

	static Keyboard* GetKeyboard(int _id = 0);
	static Mouse*    GetMouse(int _id = 0);
	static Gamepad*  GetGamepad(int _id = 0);

	// Reset device to 'default' state. Use to disable input during a polling
	// frame (e.g. if the device was consumed by a UI event).
	static void ResetKeyboard(int _id = 0)   { GetKeyboard(_id)->reset(); }
	static void ResetMouse(int _id = 0)      { GetMouse(_id)->reset(); }
	static void ResetGamepad(int _id = 0)    { GetGamepad(_id)->reset(); }
	
	static void Init();
	static void Shutdown();

}; // class Input
APT_DECLARE_STATIC_INIT(Input, Input::Init, Input::Shutdown);

class ProxyDevice
{
protected:
	int m_id;

	void setButtonState(Device* device_, int _button, bool _isDown)
	{
		device_->setIncButton(_button, _isDown);
	}
	void setAxisState(Device* device_, int _axis, float _state)
	{
		APT_ASSERT(_axis < device_->m_axisCount);
		device_->m_axisStates[_axis] = _state;
	}

public:
	ProxyDevice(int _id)
		: m_id(_id)
	{
	}
};

class ProxyKeyboard: public ProxyDevice
{
public:
	ProxyKeyboard(int _id)
		: ProxyDevice(_id)
	{
		APT_ASSERT(m_id < Input::kMaxKeyboardCount);
	}

	void setButtonState(Keyboard::Button _button, bool _isDown)
	{
		ProxyDevice::setButtonState(Input::GetKeyboard(m_id), _button, _isDown);
	}
};

class ProxyMouse: public ProxyDevice
{
public:
	ProxyMouse(int _id)
		: ProxyDevice(_id)
	{
		APT_ASSERT(m_id < Input::kMaxMouseCount);
	}
	
	void setButtonState(Mouse::Button _button, bool _isDown)
	{
		ProxyDevice::setButtonState(Input::GetMouse(m_id), _button, _isDown);
	}
	void setAxisState(Mouse::Axis _axis, float _state)
	{
		ProxyDevice::setAxisState(Input::GetMouse(m_id), _axis, _state);
	}
};

class ProxyGamepad: public ProxyDevice
{
public:
	ProxyGamepad(int _id)
		: ProxyDevice(_id)
	{
		APT_ASSERT(m_id < Input::kMaxGamepadCount);
	}
	
	void setButtonState(Gamepad::Button _button, bool _isDown)
	{
		ProxyDevice::setButtonState(Input::GetGamepad(m_id), _button, _isDown);
	}
	void setAxisState(Gamepad::Axis _axis, float _state)
	{
		ProxyDevice::setAxisState(Input::GetGamepad(m_id), _axis, _state);
	}
};

} // namespace frm

#endif // frm_Input_h

