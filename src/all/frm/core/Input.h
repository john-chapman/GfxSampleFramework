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
	enum Key
	{
		Key_Unmapped = 0, // valid button with an undefined state (must have index 0)

	 // function keys
		Key_Escape, Key_Pause, Key_PrintScreen, Key_CapsLock, Key_NumLock, Key_ScrollLock, Key_Insert, Key_Delete, Key_Clear,
		Key_LShift, Key_LCtrl, Key_LAlt, Key_RShift, Key_RCtrl, Key_RAlt,
		Key_F1, Key_F2, Key_F3, Key_F4, Key_F5, Key_F6, Key_F7, Key_F8, Key_F9, Key_F10, Key_F11, Key_F12,
		
	 // cursor control
		Key_Space, Key_Backspace, Key_Return, Key_Tab,
		Key_PageUp, Key_PageDown, Key_Home, Key_End,
		Key_Up, Key_Down, Key_Left, Key_Right,
		
	 // character keys
		Key_A, Key_B, Key_C, Key_D, Key_E, Key_F, Key_G, Key_H, Key_I, Key_J, Key_K, Key_L, Key_M, Key_N, Key_O, Key_P, Key_Q, Key_R, Key_S, Key_T, Key_U, Key_V, Key_W, Key_X, Key_Y, Key_Z,
		Key_0, Key_1, Key_2, Key_3, Key_4, Key_5, Key_6, Key_7, Key_8, Key_9,
		Key_Plus, Key_Minus, Key_Period, Key_Comma,
		Key_Misc0, Key_Misc1, Key_Misc2, Key_Misc3, Key_Misc4, Key_Misc5, Key_Misc6, Key_Misc7, // layout-specific
		
	 // numpad
		Key_Numpad0, Key_Numpad1, Key_Numpad2, Key_Numpad3, Key_Numpad4, Key_Numpad5, Key_Numpad6, Key_Numpad7, Key_Numpad8, Key_Numpad9,
		Key_NumpadEnter, Key_NumpadPlus, Key_NumpadMinus, Key_NumpadMultiply, Key_NumpadDivide, Key_NumpadPeriod,

		Key_Count,
		
	 // ranges
		Key_Function_Begin  = Key_Escape,  Key_Function_End  = Key_Space,
		Key_Cursor_Begin    = Key_Space,   Key_Cursor_End    = Key_A,
		Key_Character_Begin = Key_A,       Key_Character_End = Key_Count, // characters include numpad
		Key_Numpad_Begin    = Key_Numpad0, Key_Numpad_End    = Key_Count
	};

	static bool IsFunctionKey(Key _key)     { return _key >= Key_Function_Begin && _key < Key_Function_End;   }
	static bool IsCursorKey(Key _key)       { return _key >= Key_Cursor_Begin && _key < Key_Cursor_End;       }
	static bool IsCharacterKey(Key _key)    { return _key >= Key_Character_Begin && _key < Key_Character_End; }
	static bool IsNumpadKey(Key _key)       { return _key >= Key_Numpad_Begin && _key < Key_Numpad_End;       }

	// \note This interface is unreliable for anything other than alpha numeric chars.
	static char ToChar(Key _key);
	static Key  FromChar(char _c);

	static const char* GetKeyName(Key _key) { return s_keyNames[_key]; }

protected:
	Keyboard(): Device(Key_Count, 0) {}

	static const char* s_keyNames[Key_Count];
	static void InitKeyNames();

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
		Button_Left, 
		Button_Middle,
		Button_Right,
		
		Button_Count
	};
	enum Axis
	{
		Axis_X, 
		Axis_Y, 
		Axis_Wheel, 

		Axis_Count
	};

	static const char* GetButtonName(Button _button) { return s_buttonNames[_button]; }

protected:
	Mouse(): Device(Button_Count, Axis_Count) {}

	static const char* s_buttonNames[Button_Count];
	static void InitButtonNames();
	
	// Mouse zeroes axis states at the beginning of each polling frame.
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
		Button_Unmapped = 0,

	 // XBox/default
		Button_A, Button_B, Button_X, Button_Y,
		Button_Up, Button_Down, Button_Left, Button_Right,
		Button_Left1, Button_Left2, Button_Left3, Button_Right1, Button_Right2, Button_Right3,
		Button_Start, Button_Back,

		Button_Count,

	 // Playstation
		Button_Cross = Button_A, Button_Circle = Button_B, Button_Square = Button_X, Button_Triangle = Button_Y,
		Button_Select = Button_Back,
	};
	enum Axis
	{
		Axis_Unmapped = 0,

		Axis_LeftStickX, Axis_LeftStickY, Axis_RightStickX, Axis_RightStickY,
		Axis_LeftTrigger, Axis_RightTrigger,
		Axis_Pitch, Axis_Yaw, Axis_Roll,
		Axis_TouchpadX, Axis_TouchpadY,

		Axis_Count
	};

	static const char* GetButtonName(Button _button) { return s_buttonNames[_button]; }

protected:
	float m_deadZone;

	Gamepad()
		: Device(Button_Count, Axis_Count)
		, m_deadZone(0.2f)
	{
	}

	static const char* s_buttonNames[Button_Count];
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

	ProxyDevice(int _id)
		: m_id(_id)
	{
	}

public:
	int  getId() const  { return m_id; }
	void setId(int _id) { m_id = _id;  }

};

class ProxyKeyboard: public ProxyDevice
{
public:
	ProxyKeyboard(int _id = 0)
		: ProxyDevice(_id)
	{
		APT_ASSERT(m_id < Input::kMaxKeyboardCount);
	}

	void setButtonState(Keyboard::Key _button, bool _isDown)
	{
		ProxyDevice::setButtonState(Input::GetKeyboard(m_id), _button, _isDown);
	}
};

class ProxyMouse: public ProxyDevice
{
public:
	ProxyMouse(int _id = 0)
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
	ProxyGamepad(int _id = 0)
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

