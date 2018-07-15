#include <frm/Input.h>

#include <cstring>

using namespace frm;

/*******************************************************************************

                                  Device

*******************************************************************************/

// PUBLIC

// PROTECTED

Device::Device(int _buttonCount, int _axisCount)
	: m_deltaTime(0.0f)
	, m_isConnected(false)
	, m_buttonStates(0)
	, m_buttonCount(0)
	, m_axisStates(0)
	, m_axisCount(0)
{
	setButtonCount(_buttonCount);
	setAxisCount(_axisCount);
	//m_lastPoll = apt::Time::GetTimestamp();
	reset();
}

Device::~Device()
{
	delete[] m_axisStates;
	delete[] m_buttonStates;
}

void Device::pollBegin()
{
	//apt::Timestamp t = apt::Time::GetTimestamp();
	//m_deltaTime = (float)(t - m_lastPoll).asSeconds();
	//m_lastPoll = t;

 // zero button counters but preserve state
	for (int i = 0; i < m_buttonCount; ++i) {
		m_buttonStates[i] &= 0x80;
	}
 // zero axis states
 // \note do this per-device (some devices don't update the axes continuously)
	//memset(m_axisStates, 0, m_axisCount * sizeof(float));
}

void Device::pollEnd()
{
}

void Device::setIncButton(int _button, bool _isDown)
{
	char state = m_buttonStates[_button];
	if ((state & 0x80) && !_isDown) {
		state = ((state & 0x7f) + 1);
	}
	state = (_isDown ? 0x80 : 0x00) | (state & 0x7f);
	m_buttonStates[_button] = state;
}

void Device::reset()
{
	memset(m_buttonStates, 0, m_buttonCount * sizeof(char));
	memset(m_axisStates, 0, m_axisCount * sizeof(float));
}

void Device::setButtonCount(int _count)
{
	delete[] m_buttonStates;
	if (_count > 0u) {
		m_buttonStates = new char[_count];
		APT_ASSERT(m_buttonStates);
		m_buttonCount = _count;
	}
}

void Device::setAxisCount(int _count)
{
	delete[] m_axisStates;
	if (_count > 0u) {
		m_axisStates = new float[_count];
		APT_ASSERT(m_axisStates);
		m_axisCount = _count;
	}
}


#define SET_BUTTON_NAME(_name) s_buttonNames[Button_ ## _name] = #_name;
#define SET_KEY_NAME(_name) s_keyNames[Key_ ## _name] = #_name;

const char* Keyboard::s_keyNames[Keyboard::Key_Count];
void Keyboard::InitKeyNames()
{
	SET_KEY_NAME(Unmapped);

 // function keys
	SET_KEY_NAME(Escape);
	SET_KEY_NAME(Pause);
	SET_KEY_NAME(PrintScreen);
	SET_KEY_NAME(CapsLock);
	SET_KEY_NAME(NumLock);
	SET_KEY_NAME(ScrollLock);
	SET_KEY_NAME(Insert);
	SET_KEY_NAME(Delete);
	SET_KEY_NAME(Clear);
	SET_KEY_NAME(LShift);
	SET_KEY_NAME(LCtrl);
	SET_KEY_NAME(LAlt);
	SET_KEY_NAME(RShift);
	SET_KEY_NAME(RCtrl);
	SET_KEY_NAME(RAlt);
	SET_KEY_NAME(F1); 
	SET_KEY_NAME(F2); 
	SET_KEY_NAME(F3);
	SET_KEY_NAME(F4);
	SET_KEY_NAME(F5);
	SET_KEY_NAME(F6);
	SET_KEY_NAME(F7);
	SET_KEY_NAME(F8);
	SET_KEY_NAME(F9);
	SET_KEY_NAME(F10);
	SET_KEY_NAME(F11);
	SET_KEY_NAME(F12);

 // cursor control
	SET_KEY_NAME(Space);
	SET_KEY_NAME(Backspace);
	SET_KEY_NAME(Return);
	SET_KEY_NAME(Tab);
	SET_KEY_NAME(PageUp);
	SET_KEY_NAME(PageDown);
	SET_KEY_NAME(Home);
	SET_KEY_NAME(End);
	SET_KEY_NAME(Up);
	SET_KEY_NAME(Down);
	SET_KEY_NAME(Left);
	SET_KEY_NAME(Right);
		
 // character keys
	SET_KEY_NAME(A);
	SET_KEY_NAME(B);
	SET_KEY_NAME(C);
	SET_KEY_NAME(D);
	SET_KEY_NAME(E);
	SET_KEY_NAME(F);
	SET_KEY_NAME(G);
	SET_KEY_NAME(H);
	SET_KEY_NAME(I);
	SET_KEY_NAME(J);
	SET_KEY_NAME(K);
	SET_KEY_NAME(L);
	SET_KEY_NAME(M);
	SET_KEY_NAME(N);
	SET_KEY_NAME(O);
	SET_KEY_NAME(P);
	SET_KEY_NAME(Q);
	SET_KEY_NAME(R);
	SET_KEY_NAME(S);
	SET_KEY_NAME(T);
	SET_KEY_NAME(U);
	SET_KEY_NAME(V);
	SET_KEY_NAME(W);
	SET_KEY_NAME(X);
	SET_KEY_NAME(Y);
	SET_KEY_NAME(Z);
	SET_KEY_NAME(1);
	SET_KEY_NAME(2);
	SET_KEY_NAME(3);
	SET_KEY_NAME(4);
	SET_KEY_NAME(5);
	SET_KEY_NAME(6);
	SET_KEY_NAME(7);
	SET_KEY_NAME(8);
	SET_KEY_NAME(9);
	SET_KEY_NAME(0);
	SET_KEY_NAME(Plus);
	SET_KEY_NAME(Minus);
	SET_KEY_NAME(Period);
	SET_KEY_NAME(Comma);
	SET_KEY_NAME(Misc0);
	SET_KEY_NAME(Misc1);
	SET_KEY_NAME(Misc2);
	SET_KEY_NAME(Misc3);
	SET_KEY_NAME(Misc4);
	SET_KEY_NAME(Misc5);
	SET_KEY_NAME(Misc6);
	SET_KEY_NAME(Misc7);

 // numpad
	SET_KEY_NAME(Numpad0);
	SET_KEY_NAME(Numpad1);
	SET_KEY_NAME(Numpad2);
	SET_KEY_NAME(Numpad3);
	SET_KEY_NAME(Numpad4);
	SET_KEY_NAME(Numpad5);
	SET_KEY_NAME(Numpad6);
	SET_KEY_NAME(Numpad7);
	SET_KEY_NAME(Numpad8);
	SET_KEY_NAME(Numpad9);
	SET_KEY_NAME(NumpadEnter);
	SET_KEY_NAME(NumpadPlus);
	SET_KEY_NAME(NumpadMinus);
	SET_KEY_NAME(NumpadMultiply);
	SET_KEY_NAME(NumpadDivide);
	SET_KEY_NAME(NumpadPeriod);
}

char Keyboard::ToChar(Key _key)
{
	if (_key >= Key_A && _key <= Key_Z) {
		return 'A' + (_key - Key_A);
	}
	if (_key >= Key_0 && _key <= Key_9) {
		return '0' + (_key - Key_0);
	}
	if (_key >= Key_Numpad0 && _key <= Key_Numpad9) {
		return '0' + (_key - Key_Numpad0);
	}
	switch (_key) {
		case Key_Space:           return ' ';
		case Key_Plus:
		case Key_NumpadPlus:      return '+';
		case Key_Minus:
		case Key_NumpadMinus:     return '-';
		case Key_NumpadMultiply:  return '*';
		case Key_NumpadDivide:    return '/';
		case Key_NumpadPeriod:
		case Key_Period:          return '.';
		case Key_Comma:           return ',';
		default:                  break;
	};
	return 0;
}
Keyboard::Key Keyboard::FromChar(char _c)
{
	if (_c >= 'A' && _c <= 'Z') {
		return (Key)(Key_A + (_c - 'A'));
	}
	if (_c >= 'a' && _c <= 'z') {
		return (Key)(Key_A + (_c - 'a'));
	}
	if (_c >= '0' && _c <= '9') {
		return (Key)(Key_0 + (_c - '0'));
	}
	switch (_c) {
		case ' ': return Key_Space;
		case '+': return Key_Plus;
		case '-': return Key_Minus;
		case '*': return Key_NumpadMultiply;
		case '/': return Key_NumpadDivide;
		case '.': return Key_Period;
		case ',': return Key_Comma;
		default:  break;
	};
	return Key_Unmapped;
}

const char* Mouse::s_buttonNames[Mouse::Button_Count];
void Mouse::InitButtonNames()
{
 	SET_BUTTON_NAME(Left);
	SET_BUTTON_NAME(Middle);
	SET_BUTTON_NAME(Right);
}

void Mouse::pollBegin()
{
	Device::pollBegin();
	memset(m_axisStates, 0, m_axisCount * sizeof(float));
}

const char* Gamepad::s_buttonNames[Gamepad::Button_Count];
void Gamepad::InitButtonNames()
{
	SET_BUTTON_NAME(Unmapped);
 	SET_BUTTON_NAME(A);
	SET_BUTTON_NAME(B);
	SET_BUTTON_NAME(X);
	SET_BUTTON_NAME(Y);
	SET_BUTTON_NAME(Up);
	SET_BUTTON_NAME(Down);
	SET_BUTTON_NAME(Left);
	SET_BUTTON_NAME(Right);
	SET_BUTTON_NAME(Left1);
	SET_BUTTON_NAME(Left2);
	SET_BUTTON_NAME(Left3);
	SET_BUTTON_NAME(Right1);
	SET_BUTTON_NAME(Right2);
	SET_BUTTON_NAME(Right3);
	SET_BUTTON_NAME(Start);
	SET_BUTTON_NAME(Back);
}

/*******************************************************************************

                                  Input

*******************************************************************************/

APT_DEFINE_STATIC_INIT(Input);
