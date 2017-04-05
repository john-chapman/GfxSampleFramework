#include <frm/Input.h>

#include <frm/Window.h>
#include <frm/Profiler.h>

#include <apt/platform.h>
#include <apt/win.h>
#include <apt/log.h>

#include <new>
#include <cmath>
#include <cstring>
#include <EASTL/utility.h>

extern "C" {
	#include <hidsdi.h>
	#include <hidpi.h>
}

static const char* HIDClass_ErrStr(int _err)
{
	#define CASE_ENUM(e) case e: return #e
	switch (_err) {
		CASE_ENUM(HIDP_STATUS_SUCCESS);
		CASE_ENUM(HIDP_STATUS_NULL);
		CASE_ENUM(HIDP_STATUS_INVALID_PREPARSED_DATA);
		CASE_ENUM(HIDP_STATUS_INVALID_REPORT_TYPE);
		CASE_ENUM(HIDP_STATUS_INVALID_REPORT_LENGTH);
		CASE_ENUM(HIDP_STATUS_USAGE_NOT_FOUND);
		CASE_ENUM(HIDP_STATUS_VALUE_OUT_OF_RANGE);
		CASE_ENUM(HIDP_STATUS_BAD_LOG_PHY_VALUES);
		CASE_ENUM(HIDP_STATUS_BUFFER_TOO_SMALL);
		CASE_ENUM(HIDP_STATUS_INTERNAL_ERROR);
		CASE_ENUM(HIDP_STATUS_I8042_TRANS_UNKNOWN);
		CASE_ENUM(HIDP_STATUS_INCOMPATIBLE_REPORT_ID);
		CASE_ENUM(HIDP_STATUS_NOT_VALUE_ARRAY);
		CASE_ENUM(HIDP_STATUS_IS_VALUE_ARRAY);
		CASE_ENUM(HIDP_STATUS_DATA_INDEX_NOT_FOUND);
		CASE_ENUM(HIDP_STATUS_DATA_INDEX_OUT_OF_RANGE);
		CASE_ENUM(HIDP_STATUS_BUTTON_NOT_PRESSED);
		CASE_ENUM(HIDP_STATUS_REPORT_DOES_NOT_EXIST);
		CASE_ENUM(HIDP_STATUS_NOT_IMPLEMENTED);
		default: return "Unknown HIDP_STATUS enum";
	};
	#undef CASE_ENUM
}
#define HIDClass_VERIFY(call) { \
		int err = (call); \
		APT_ASSERT_MSG(err == HIDP_STATUS_SUCCESS, HIDClass_ErrStr(err)); \
	}

using namespace frm;

/*******************************************************************************

                                  Input

*******************************************************************************/
static const HANDLE kNullHandle = (HANDLE)0;
static HWND s_inputWindow;

/* Per-device implementation classes inherit from ImplBase, which inherits from
   the base device (Keyboard, Mouse, Gamepad) via tDevice. The impl class
   is also a template to ImplBase (via tImpl) as we need to store the instance 
   list and also cast to a tImpl* in some cases. Basically ImplBase needs to call
   both up and down in the hierarchy so we either need template magic or 
   virtual functions, I chose template magic. The final hierarchy is effctively:

   Device  <-  Mouse  <-  ImplBase<Mouse, MouseImpl>  <-  MouseImpl
*/
template <typename tDevice, typename tImpl, int kMaxCount>
struct ImplBase: public tDevice
{
	static tImpl s_instances[kMaxCount];
	HANDLE m_handle;
	
	static tImpl* Find(HANDLE _handle)
	{
		for (int i = 0; i < kMaxCount; ++i) {
			if (s_instances[i].m_handle == _handle) {
				return &s_instances[i];
			}
		}
		return 0;
	}

	static void PollBegin()
	{
		for (int i = 0; i < kMaxCount; ++i) {
			//if (s_instances[i].isConnected()) { // process all devices to account for proxies
				s_instances[i].pollBegin();
			//}
		}
	}

	static void PollEnd()
	{
		for (int i = 0; i < kMaxCount; ++i) {
			//if (s_instances[i].isConnected()) { // process all devices to account for proxies
				s_instances[i].pollEnd();
			//}
		}
	}

	ImplBase()
		: m_handle(kNullHandle)
	{
	}

	unsigned getIndex() const
	{
		return (unsigned)((tImpl*)this - s_instances);
	}

	const char* getName() const
	{
		return ((tImpl*)this)->getName();
	}

	void connect(RAWINPUT* _raw)
	{
		HANDLE h = _raw->header.hDevice;
		APT_ASSERT_MSG(Find(h) == 0, "%s 0x%x already connected", getName(), h);
		APT_LOG("%s %d connected (0x%x)", getName(), getIndex(), h);
		m_handle = h;
		m_isConnected = true;
		reset();
	}

	void disconnect()
	{
		APT_LOG("%s %d disconnected (0x%x)", getName(), getIndex(), m_handle);
		m_handle = kNullHandle;
		m_isConnected = false;
		if (this == s_instances) {
		 // default was disconnected, try to move another mouse to this slot
			for (int i = 1; i < Input::kMaxMouseCount; ++i) {
				if (s_instances[i].m_handle != kNullHandle) {
					APT_LOG("%s %d (0x%x) moved to index 0", getName(), i, s_instances[i].m_handle);
					eastl::swap(s_instances[i].m_handle, s_instances[0].m_handle);
					s_instances[0].reset(); // we didn't bother to swap the state as well, so reset the device
					break;
				}
			}
		}
	}

}; // struct ImplBase
template <typename tDevice, typename tImpl, int kMaxCount>
tImpl ImplBase<tDevice, tImpl, kMaxCount>::s_instances[kMaxCount];

struct KeyboardImpl: public ImplBase<Keyboard, KeyboardImpl, Input::kMaxKeyboardCount>
{
	typedef ImplBase<Keyboard, KeyboardImpl, Input::kMaxKeyboardCount> Base;

	const char* getName() const
	{
		return "Keyboard";
	}

	void update(RAWINPUT* _raw)
	{
	 // adapted from https://blog.molecular-matters.com/2011/09/05/properly-handling-keyboard-input/

		UINT vk    = _raw->data.keyboard.VKey;
		UINT sc    = _raw->data.keyboard.MakeCode;
		UINT flags = _raw->data.keyboard.Flags;
		Key key = Key_Unmapped;

		if (vk == 255) {
		 // discard 'fake' escape sequence keys
			return;
		}

		bool e0 = (flags & RI_KEY_E0) != 0;
		bool e1 = (flags & RI_KEY_E1) != 0;

		if (vk >= 0x41 && vk <= 0x5A) {
		 // alpha
			key = (Key)(Key_A + vk - 0x41);
		} else if (vk >= 0x30 && vk <= 0x39) {
		 // numeric
			key = vk == 0x30 ? Key_0 : (Key)(Key_1 + vk - 0x31);
		} else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
		 // numpad numeric
			key  = (Key)(Key_Numpad0 + vk - VK_NUMPAD0);
		} else if (vk >= VK_F1 && vk <= VK_F12) {
		 // F keys
			key = (Key)(Key_F1 + vk - VK_F1);
		} else {
		 // all other keys

			if (vk == VK_SHIFT) {
				vk = MapVirtualKey(sc, MAPVK_VSC_TO_VK_EX);
			}

			switch (vk) {
			 	case VK_CONTROL:     key = e0 ? Key_RCtrl       : Key_LCtrl;        break;
				case VK_MENU:        key = e0 ? Key_RAlt        : Key_LAlt;         break;
				case VK_RETURN:      key = e0 ? Key_NumpadEnter : Key_Return;       break;
				case VK_INSERT:      key = e0 ? Key_Insert      : Key_Numpad0;      break;
				case VK_END:         key = e0 ? Key_End         : Key_Numpad1;      break;
				case VK_DOWN:        key = e0 ? Key_Down        : Key_Numpad2;      break;
				case VK_NEXT:        key = e0 ? Key_PageDown    : Key_Numpad3;      break;
				case VK_LEFT:        key = e0 ? Key_Left        : Key_Numpad4;      break;
				case VK_CLEAR:       key = e0 ? Key_Clear       : Key_Numpad5;      break;
				case VK_RIGHT:       key = e0 ? Key_Right       : Key_Numpad6;      break;
				case VK_HOME:        key = e0 ? Key_Home        : Key_Numpad7;      break;
				case VK_UP:          key = e0 ? Key_Up          : Key_Numpad8;      break;
				case VK_PRIOR:       key = e0 ? Key_PageUp      : Key_Numpad9;      break;
				case VK_DELETE:      key = e0 ? Key_Delete      : Key_NumpadPeriod; break;
				
			 	case VK_LSHIFT:      key = Key_LShift;         break;
				case VK_RSHIFT:      key = Key_RShift;         break;
				case VK_ESCAPE:      key = Key_Escape;         break;
				case VK_PAUSE:       key = Key_Pause;          break;
				case VK_SNAPSHOT:    key = Key_PrintScreen;    break;
				case VK_CAPITAL:     key = Key_CapsLock;       break;
				case VK_NUMLOCK:     key = Key_NumLock;        break;
				case VK_DECIMAL:     key = Key_NumpadPeriod;   break;
				case VK_ADD:         key = Key_NumpadPlus;     break;
				case VK_SUBTRACT:    key = Key_NumpadMinus;    break;
				case VK_DIVIDE:      key = Key_NumpadDivide;   break;
				case VK_MULTIPLY:    key = Key_NumpadMultiply; break;
				case VK_SCROLL:      key = Key_ScrollLock;     break;
				case VK_SPACE:       key = Key_Space;          break;
				case VK_BACK:        key = Key_Backspace;      break;
				case VK_TAB:         key = Key_Tab;            break;
				case VK_OEM_PLUS:    key = Key_Plus;           break;
				case VK_OEM_MINUS:   key = Key_Minus;          break;
				case VK_OEM_PERIOD:  key = Key_Period;         break;
				case VK_OEM_COMMA:   key = Key_Comma;          break;
				case VK_OEM_1:       key = Key_Misc0;          break;
				case VK_OEM_2:       key = Key_Misc1;          break;
				case VK_OEM_3:       key = Key_Misc2;          break;
				case VK_OEM_4:       key = Key_Misc3;          break;
				case VK_OEM_5:       key = Key_Misc4;          break;
				case VK_OEM_6:       key = Key_Misc5;          break;
				case VK_OEM_7:       key = Key_Misc6;          break;
				case VK_OEM_8:       key = Key_Misc7;          break;

				default: break;
			};
		}

		setIncButton(key, (flags & RI_KEY_BREAK) == 0u);

		//APT_LOG("%s e0=%i, e1=%i, f=0x%x, sc=0x%x", GetButtonName(button), (int)e0, (int)e1, flags, sc);
	}

}; // struct KeyboardImpl

struct MouseImpl: public ImplBase<Mouse, MouseImpl, Input::kMaxMouseCount>
{
	typedef ImplBase<Mouse, MouseImpl, Input::kMaxMouseCount> Base;

	const char* getName() const
	{
		return "Mouse";
	}

	void update(RAWINPUT* _raw)
	{
		APT_ASSERT_MSG((_raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0, "%s %d does not support relative position coordinates", getName(), getIndex());
	 
	 // buttons
		USHORT currFlags = _raw->data.mouse.usButtonFlags;
		if (currFlags != 0) {
			setIncButton(Mouse::Button_Left,   (_raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)   != 0);
			setIncButton(Mouse::Button_Middle, (_raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) != 0);
			setIncButton(Mouse::Button_Right,  (_raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)  != 0);
		}

	 // axes
		if (_raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
			m_axisStates[Mouse::Axis_Wheel] += (float)((SHORT)_raw->data.mouse.usButtonData);
		}
		m_axisStates[Mouse::Axis_X] += (float)_raw->data.mouse.lLastX;
		m_axisStates[Mouse::Axis_Y] += (float)_raw->data.mouse.lLastY;
	}

}; // struct MouseImpl

struct GamepadImpl: public ImplBase<Gamepad, GamepadImpl, Input::kMaxGamepadCount>
{
	typedef ImplBase<Gamepad, GamepadImpl, Input::kMaxGamepadCount> Base;

	PHIDP_PREPARSED_DATA m_preparsedData;
	UINT m_preparsedDataLength;
	PHIDP_BUTTON_CAPS m_buttonCaps;
	ULONG m_buttonUsageCount;
	PUSAGE m_buttonUsage[2];
	PUSAGE m_buttonMake, m_buttonBreak;
	UINT m_currentUsage;

	PHIDP_VALUE_CAPS m_valueCaps;
	ULONG m_valueUsageCount;


	enum Type
	{
		Type_XBox360,
		Type_Ps4DualShock4,

		Type_Count
	};
	Type m_type;

	static Button HidUsageToButton(int _hid, Type _type)
	{
		static const Button kButtonMap[Type_Count][Button_Count] = {
			{ Button_A, Button_B, Button_X, Button_Y, Button_Left1, Button_Right1, Button_Back, Button_Start, Button_Left3, Button_Right3, }, // Button_XBox360
			{ Button_X, Button_A, Button_B, Button_Y, Button_Left1, Button_Right1, Button_Left2, Button_Right2, Button_Unmapped, Button_Start, Button_Left3, Button_Right3, Button_Unmapped, Button_Back }, // Button_Ps4DualShock4
		};
		return kButtonMap[_type][_hid];
	}
	static Axis HidUsageToAxis(int _hid, Type _type)
	{
		static const Axis kAxisMap[Type_Count][Axis_Count] = {
			{ Axis_LeftStickY, Axis_LeftStickX, Axis_RightStickY, Axis_RightStickX, Axis_Unmapped/*triggers*/, Axis_Unmapped/*dpad*/, }, // Axis_XBox360
			{ Axis_RightStickY, Axis_RightStickX, Axis_LeftStickY, Axis_LeftStickX, Axis_Unmapped/*dpad*/, Axis_Unmapped, Axis_RightTrigger, Axis_LeftTrigger, Axis_Unmapped },  // Axis_Ps4DualShock4
		};
		return kAxisMap[_type][_hid];
	}

	const char* getName() const
	{
		return "Gamepad";
	}

	void connect(RAWINPUT* _raw)
	{
		Base::connect(_raw);
		
		RID_DEVICE_INFO deviceInfo;
		UINT deviceInfoSize = sizeof(deviceInfo);
		APT_PLATFORM_VERIFY(GetRawInputDeviceInfo(_raw->header.hDevice, RIDI_DEVICEINFO, &deviceInfo, &deviceInfoSize) >= 0);
		m_type = Type_Count;
	 // http://www.linux-usb.org/usb.ids
		switch (deviceInfo.hid.dwVendorId) {
			case 0x45e: // Microsoft
				switch (deviceInfo.hid.dwProductId) {
					case 0x28e: m_type = Type_XBox360; break;
					case 0x28f: // XBox360 wireless
					case 0x2d1: // XBoxOne
					default:
						break;
				};
			case 0x54c: // Sony
				switch (deviceInfo.hid.dwProductId) {
					case 0x9cc: // model 1E
					case 0x5c4: m_type = Type_Ps4DualShock4; break; // model 2E
					default:
						break;
				};
			default:
				break;
		};	
	 // automatically disconnect any unsupported devices
		if (m_type == Type_Count) {
			APT_LOG_ERR("Unsupported gamepad type (vendor %d product %d)", deviceInfo.hid.dwVendorId, deviceInfo.hid.dwProductId);
			disconnect();
			return;
		}

		APT_PLATFORM_VERIFY(GetRawInputDeviceInfo(_raw->header.hDevice, RIDI_PREPARSEDDATA, NULL, &m_preparsedDataLength) == 0);
		m_preparsedData = (PHIDP_PREPARSED_DATA)new BYTE[m_preparsedDataLength];
		APT_PLATFORM_VERIFY(GetRawInputDeviceInfo(_raw->header.hDevice, RIDI_PREPARSEDDATA, m_preparsedData, &m_preparsedDataLength) >= 0);

		HIDP_CAPS caps;
		HIDClass_VERIFY(HidP_GetCaps(m_preparsedData, &caps));

		USHORT bccount = caps.NumberInputButtonCaps;
		m_buttonCaps = (PHIDP_BUTTON_CAPS)new HIDP_BUTTON_CAPS[bccount];
		HIDClass_VERIFY(HidP_GetButtonCaps(HidP_Input, m_buttonCaps, &bccount, m_preparsedData));
		m_buttonUsageCount = m_buttonCaps[0].Range.UsageMax - m_buttonCaps[0].Range.UsageMin + 1;
		m_buttonUsage[0] = new USAGE[m_buttonUsageCount];
		m_buttonUsage[1] = new USAGE[m_buttonUsageCount];
		m_buttonMake = new USAGE[m_buttonUsageCount];
		m_buttonBreak = new USAGE[m_buttonUsageCount];
		ZeroMemory(m_buttonUsage[0], sizeof(USAGE) * m_buttonUsageCount);
		ZeroMemory(m_buttonUsage[1], sizeof(USAGE) * m_buttonUsageCount);

		m_valueUsageCount = caps.NumberInputValueCaps;
		USHORT vccount = (USHORT)m_valueUsageCount;
		m_valueCaps = (PHIDP_VALUE_CAPS)new HIDP_VALUE_CAPS[vccount];
		HIDClass_VERIFY(HidP_GetValueCaps(HidP_Input, m_valueCaps, &vccount, m_preparsedData));
		if (m_type == Type_XBox360) {
		 // xbox doesn't supply the logical ranges but they're all USHORT
			for (ULONG i = 0; i < m_valueUsageCount; ++i) {
				m_valueCaps[i].LogicalMin = 0;
				m_valueCaps[i].LogicalMax = USHRT_MAX;
			}
		}
	}

	void disconnect()
	{
		Base::disconnect();
		delete[] (BYTE*)m_preparsedData;
		delete[] m_buttonCaps;
		delete[] m_buttonUsage[0];
		delete[] m_buttonUsage[1];
		delete[] m_buttonMake;
		delete[] m_buttonBreak;
		delete[] m_valueCaps;
	
	}

	void update(RAWINPUT* _raw)
	{
		APT_PLATFORM_VERIFY(GetRawInputDeviceInfo(_raw->header.hDevice, RIDI_PREPARSEDDATA, m_preparsedData, &m_preparsedDataLength) >= 0);

	 // buttons
		ULONG usageCount = m_buttonUsageCount;
		HIDClass_VERIFY(HidP_GetButtons(HidP_Input, m_buttonCaps->UsagePage, 0, m_buttonUsage[m_currentUsage], &usageCount, m_preparsedData, (PCHAR)_raw->data.hid.bRawData, _raw->data.hid.dwSizeHid));
		
		UINT prevUsage = (m_currentUsage + 1) % 2;
		HIDClass_VERIFY(HidP_UsageListDifference(m_buttonUsage[prevUsage], m_buttonUsage[m_currentUsage], m_buttonBreak, m_buttonMake, m_buttonUsageCount));
		for (UINT i = 0; i < m_buttonUsageCount; ++i) {
			if (m_buttonMake[i] != 0) {
				int b = m_buttonMake[i] - m_buttonCaps->Range.UsageMin;
				setIncButton(HidUsageToButton(b, m_type), true);
			}
			if (m_buttonBreak[i] != 0) {
				int b = m_buttonBreak[i] - m_buttonCaps->Range.UsageMin;
				setIncButton(HidUsageToButton(b, m_type), false);
			}
		}
		m_currentUsage = prevUsage;
		
	 // axes
		for (USHORT i = 0; i < m_valueUsageCount; ++i) {
			ULONG val;
			HIDClass_VERIFY(HidP_GetUsageValue(HidP_Input, m_valueCaps[i].UsagePage, 0, m_valueCaps[i].Range.UsageMin, &val, m_preparsedData, (PCHAR)_raw->data.hid.bRawData, _raw->data.hid.dwSizeHid));

			Axis ax = HidUsageToAxis(i, m_type);
			if (ax == Axis_Unmapped) {
			 // special cases:
			 // dpad: on xbox/playstation the dpad is an axis with 8 values; we convert these to the 4 cardinal buttons (up/down/left/right).
			 // triggers: on xbox both triggers share an axes, 
				switch (m_type) {
					case Type_XBox360:
						switch (i) {
							case 4: { // trigger
								float fval = (float)(val - m_valueCaps[i].LogicalMin) / (float)m_valueCaps[i].LogicalMax;
								fval = (fval <= m_deadZone) ? 0.0f : fval;
								m_axisStates[Axis_LeftTrigger]  = APT_CLAMP((fval - 0.5f) * 2.0f, 0.0f, 1.0f);
								m_axisStates[Axis_RightTrigger] = 1.0f - APT_CLAMP(fval * 2.0f, 0.0f, 1.0f);
								setIncButton(Button_Left2,  m_axisStates[Axis_LeftTrigger] > 0.5f);
								setIncButton(Button_Right2, m_axisStates[Axis_RightTrigger] > 0.5f);
								break;
							};
							case 5: // dpad
								setIncButton(Button_Up,    val == 8 || val == 1 || val == 2);
								setIncButton(Button_Right, val == 2 || val == 3 || val == 4);
								setIncButton(Button_Down,  val == 4 || val == 5 || val == 6);
								setIncButton(Button_Left,  val == 6 || val == 7 || val == 8);
								break;
							default:
								break;
						};
						break;
					case Type_Ps4DualShock4:
						switch (i) {
							case 4: // dpad
								setIncButton(Button_Up,    val == 7 || val == 0 || val == 1);
								setIncButton(Button_Right, val == 1 || val == 2 || val == 3);
								setIncButton(Button_Down,  val == 3 || val == 4 || val == 5);
								setIncButton(Button_Left,  val == 5 || val == 6 || val == 7);
								break;
							default:
								break;
						};
						break;
					default:
						break;
				};
				
				continue;
			} 
			float fval = (float)(val - m_valueCaps[i].LogicalMin) / (float)m_valueCaps[i].LogicalMax;
			if (ax != Axis_LeftTrigger && ax != Axis_RightTrigger) {
			 // move non-trigger axes to [-1,1]
				fval = fval * 2.0f - 1.0f;
			}
			fval = (fabs(fval) <= m_deadZone) ? 0.0f : fval;
			if (m_valueCaps[i].IsAbsolute) {
			 // set absolute values
				m_axisStates[ax] = fval;
			} else {
			 // combine relative values
				m_axisStates[ax] += fval;
			}
		}
	}

}; // struct GamepadImpl



static LRESULT CALLBACK InputWindowProc(HWND _hwnd, UINT _umsg, WPARAM _wparam, LPARAM _lparam)
{
	static const int kMaxBufferSize = 1024;
	static BYTE buf[kMaxBufferSize];
	UINT buflen;
	RAWINPUT* raw = (RAWINPUT*)buf;
	if (_umsg == WM_INPUT) {
		APT_PLATFORM_VERIFY(GetRawInputData((HRAWINPUT)_lparam, RID_INPUT, NULL, &buflen, sizeof(RAWINPUTHEADER)) == 0);
		APT_ASSERT(buflen < kMaxBufferSize);
		APT_PLATFORM_VERIFY(GetRawInputData((HRAWINPUT)_lparam, RID_INPUT, buf, &buflen, sizeof(RAWINPUTHEADER)) == buflen);
		
		if (raw->header.dwType == RIM_TYPEMOUSE) {
			MouseImpl* mouse;
			mouse = MouseImpl::Find(raw->header.hDevice);
			if_likely (mouse) {
				mouse->update(raw);
			} else {
			 // device not found but in use, so set device 0
				APT_LOG_DBG("Set unknown device as mouse 0");
				APT_VERIFY(mouse = &MouseImpl::s_instances[0]);
				if (mouse) {
					mouse->m_handle = raw->header.hDevice;
					mouse->update(raw);
				}
			}

		} else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
			KeyboardImpl* keyboard;
			keyboard = KeyboardImpl::Find(raw->header.hDevice);
			if_likely (keyboard) {
				keyboard->update(raw);
			} else {
			 // device not found but in use, so set device 0
				APT_LOG_DBG("Set unknown device as keyboard 0");
				APT_VERIFY(keyboard = &KeyboardImpl::s_instances[0]);
				if (keyboard) {
					keyboard->m_handle = raw->header.hDevice;
					keyboard->update(raw);
				}
			}

		} else if (raw->header.dwType == RIM_TYPEHID) {
			GamepadImpl* gamepad;
			gamepad = GamepadImpl::Find(raw->header.hDevice);
			if_likely (gamepad) {
				gamepad->update(raw);
			} else {
			 // device not found but in use, so set device 0
				APT_LOG_DBG("Set unknown device as gamepad 0");
				APT_VERIFY(gamepad = &GamepadImpl::s_instances[0]);
				if (gamepad) {
					gamepad->m_handle = raw->header.hDevice;
					gamepad->update(raw);
				}
			}

		}

	} else if (_umsg == WM_INPUT_DEVICE_CHANGE) {
		if (_wparam == GIDC_ARRIVAL) {
		 // device was connected
			RID_DEVICE_INFO info;
			UINT sz = sizeof(info);
			APT_PLATFORM_VERIFY(GetRawInputDeviceInfo((HRAWINPUT)_lparam, RIDI_DEVICEINFO, &info, &sz) == sizeof(info));
			
			raw->header.hDevice = (HANDLE)_lparam; // connect() expects the handle to be stored here

			if (info.dwType == RIM_TYPEMOUSE) {
				MouseImpl* mouse = MouseImpl::Find(kNullHandle);
				if (!mouse) {
					APT_LOG("Too many mice connected, max is %d", Input::kMaxMouseCount);
				} else {
					mouse->connect(raw);
				}
			} else if (info.dwType == RIM_TYPEKEYBOARD) {
				KeyboardImpl* keyboard = KeyboardImpl::Find(kNullHandle);
				if (!keyboard) {
					APT_LOG("Too many keyboards connected, max is %d", Input::kMaxKeyboardCount);
				} else {
					keyboard->connect(raw);
				}
			} else if (info.dwType == RIM_TYPEHID && info.hid.usUsagePage == 0x01 && info.hid.usUsage == 0x05) {
				GamepadImpl* gamepad = GamepadImpl::Find(kNullHandle);
				if (!gamepad) {
					APT_LOG("Too many gamepads connected, max is %d", Input::kMaxGamepadCount);
				} else {
					gamepad->connect(raw);
				}
			}
		
		} else if (_wparam == GIDC_REMOVAL) {
		 // device was disconnected
			if (MouseImpl* mouse = MouseImpl::Find((HANDLE)_lparam)) {
				mouse->disconnect();
			} else if (KeyboardImpl* keyboard = KeyboardImpl::Find((HANDLE)_lparam)) {
				keyboard->disconnect();
			} else if (GamepadImpl* gamepad = GamepadImpl::Find((HANDLE)_lparam)) {
				gamepad->disconnect();
			}
		}

	}

	return DefWindowProc(_hwnd, _umsg, _wparam, _lparam);
}


// PUBLIC


Keyboard* Input::GetKeyboard(int _id)
{
	APT_ASSERT(_id < Input::kMaxKeyboardCount);
	return &KeyboardImpl::s_instances[_id];
}

Mouse* Input::GetMouse(int _id)
{
	APT_ASSERT(_id < Input::kMaxMouseCount);
	return &MouseImpl::s_instances[_id];
}

Gamepad* Input::GetGamepad(int _id)
{
	APT_ASSERT(_id < Input::kMaxGamepadCount);
	return &GamepadImpl::s_instances[_id];
}


// PROTECTED

void Input::Init()
{
	Keyboard::InitKeyNames();
	Mouse::InitButtonNames();
	Gamepad::InitButtonNames();

	static ATOM wndclassex = 0;
	if (wndclassex == 0) {
		WNDCLASSEX wc;
		memset(&wc, 0, sizeof(wc));
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = InputWindowProc;
		wc.hInstance = GetModuleHandle(0);
		wc.lpszClassName = "InputImpl";
		APT_PLATFORM_VERIFY(wndclassex = RegisterClassEx(&wc));
	}
	s_inputWindow = CreateWindowEx(
		0u, 
		MAKEINTATOM(wndclassex), 
		0, 
		0u, 
		0, 0, 
		0, 0, 
		HWND_MESSAGE, 
		NULL, 
		GetModuleHandle(0), 
		NULL
		);
	APT_PLATFORM_ASSERT(s_inputWindow);

	RAWINPUTDEVICE hids[3]; // 3 for keyboard, mouse, gamepad
	int currentHid = 0;

 // keyboards
	hids[currentHid].usUsagePage = 0x01;
	hids[currentHid].usUsage     = 0x06;
	hids[currentHid].dwFlags     = RIDEV_DEVNOTIFY; // | RIDEV_NOLEGACY
	hids[currentHid].hwndTarget  = s_inputWindow;
	++currentHid;

 // mice
	hids[currentHid].usUsagePage = 0x01;
	hids[currentHid].usUsage     = 0x02;
	hids[currentHid].dwFlags     = RIDEV_DEVNOTIFY; // | RIDEV_NOLEGACY
	hids[currentHid].hwndTarget  = s_inputWindow;
	++currentHid;
	
 // gamepads
	hids[currentHid].usUsagePage = 0x01;
	hids[currentHid].usUsage     = 0x05;
	hids[currentHid].dwFlags     = RIDEV_DEVNOTIFY;// | RIDEV_NOLEGACY;
	hids[currentHid].hwndTarget  = s_inputWindow;
	++currentHid;
	
	APT_PLATFORM_VERIFY(RegisterRawInputDevices(hids, currentHid, sizeof(RAWINPUTDEVICE)));
}

void Input::Shutdown()
{
	APT_PLATFORM_VERIFY(DestroyWindow(s_inputWindow));
}

void Input::PollAllDevices() 
{
	CPU_AUTO_MARKER("Input::PollAllDevices");

	KeyboardImpl::PollBegin();
	MouseImpl::PollBegin();
	GamepadImpl::PollBegin();

	MSG msg;
	while (PeekMessage(&msg, s_inputWindow, WM_INPUT_DEVICE_CHANGE, WM_INPUT, PM_REMOVE)) {
		DispatchMessage(&msg);
	}

	KeyboardImpl::PollEnd();
	MouseImpl::PollEnd();
	GamepadImpl::PollEnd();
}
