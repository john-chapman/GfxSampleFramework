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


#define SET_BUTTON_NAME(_name) s_buttonNames[k ## _name] = #_name;

const char* Keyboard::s_buttonNames[Keyboard::kButtonCount];
void Keyboard::InitButtonNames()
{
	SET_BUTTON_NAME(Unmapped);

 // function keys
	SET_BUTTON_NAME(Escape);
	SET_BUTTON_NAME(Pause);
	SET_BUTTON_NAME(PrintScreen);
	SET_BUTTON_NAME(CapsLock);
	SET_BUTTON_NAME(NumLock);
	SET_BUTTON_NAME(ScrollLock);
	SET_BUTTON_NAME(Insert);
	SET_BUTTON_NAME(Delete);
	SET_BUTTON_NAME(Clear);
	SET_BUTTON_NAME(LShift);
	SET_BUTTON_NAME(LCtrl);
	SET_BUTTON_NAME(LAlt);
	SET_BUTTON_NAME(RShift);
	SET_BUTTON_NAME(RCtrl);
	SET_BUTTON_NAME(RAlt);
	SET_BUTTON_NAME(F1); 
	SET_BUTTON_NAME(F2); 
	SET_BUTTON_NAME(F3);
	SET_BUTTON_NAME(F4);
	SET_BUTTON_NAME(F5);
	SET_BUTTON_NAME(F6);
	SET_BUTTON_NAME(F7);
	SET_BUTTON_NAME(F8);
	SET_BUTTON_NAME(F9);
	SET_BUTTON_NAME(F10);
	SET_BUTTON_NAME(F11);
	SET_BUTTON_NAME(F12);

 // cursor control
	SET_BUTTON_NAME(Space);
	SET_BUTTON_NAME(Backspace);
	SET_BUTTON_NAME(Return);
	SET_BUTTON_NAME(Tab);
	SET_BUTTON_NAME(PageUp);
	SET_BUTTON_NAME(PageDown);
	SET_BUTTON_NAME(Home);
	SET_BUTTON_NAME(End);
	SET_BUTTON_NAME(Up);
	SET_BUTTON_NAME(Down);
	SET_BUTTON_NAME(Left);
	SET_BUTTON_NAME(Right);
		
 // character keys
	SET_BUTTON_NAME(A);
	SET_BUTTON_NAME(B);
	SET_BUTTON_NAME(C);
	SET_BUTTON_NAME(D);
	SET_BUTTON_NAME(E);
	SET_BUTTON_NAME(F);
	SET_BUTTON_NAME(G);
	SET_BUTTON_NAME(H);
	SET_BUTTON_NAME(I);
	SET_BUTTON_NAME(J);
	SET_BUTTON_NAME(K);
	SET_BUTTON_NAME(L);
	SET_BUTTON_NAME(M);
	SET_BUTTON_NAME(N);
	SET_BUTTON_NAME(O);
	SET_BUTTON_NAME(P);
	SET_BUTTON_NAME(Q);
	SET_BUTTON_NAME(R);
	SET_BUTTON_NAME(S);
	SET_BUTTON_NAME(T);
	SET_BUTTON_NAME(U);
	SET_BUTTON_NAME(V);
	SET_BUTTON_NAME(W);
	SET_BUTTON_NAME(X);
	SET_BUTTON_NAME(Y);
	SET_BUTTON_NAME(Z);
	SET_BUTTON_NAME(1);
	SET_BUTTON_NAME(2);
	SET_BUTTON_NAME(3);
	SET_BUTTON_NAME(4);
	SET_BUTTON_NAME(5);
	SET_BUTTON_NAME(6);
	SET_BUTTON_NAME(7);
	SET_BUTTON_NAME(8);
	SET_BUTTON_NAME(9);
	SET_BUTTON_NAME(0);
	SET_BUTTON_NAME(Plus);
	SET_BUTTON_NAME(Minus);
	SET_BUTTON_NAME(Period);
	SET_BUTTON_NAME(Comma);
	SET_BUTTON_NAME(Misc0);
	SET_BUTTON_NAME(Misc1);
	SET_BUTTON_NAME(Misc2);
	SET_BUTTON_NAME(Misc3);
	SET_BUTTON_NAME(Misc4);
	SET_BUTTON_NAME(Misc5);
	SET_BUTTON_NAME(Misc6);
	SET_BUTTON_NAME(Misc7);

 // numpad
	SET_BUTTON_NAME(Numpad0);
	SET_BUTTON_NAME(Numpad1);
	SET_BUTTON_NAME(Numpad2);
	SET_BUTTON_NAME(Numpad3);
	SET_BUTTON_NAME(Numpad4);
	SET_BUTTON_NAME(Numpad5);
	SET_BUTTON_NAME(Numpad6);
	SET_BUTTON_NAME(Numpad7);
	SET_BUTTON_NAME(Numpad8);
	SET_BUTTON_NAME(Numpad9);
	SET_BUTTON_NAME(NumpadEnter);
	SET_BUTTON_NAME(NumpadPlus);
	SET_BUTTON_NAME(NumpadMinus);
	SET_BUTTON_NAME(NumpadMultiply);
	SET_BUTTON_NAME(NumpadDivide);
	SET_BUTTON_NAME(NumpadPeriod);
}

const char* Mouse::s_buttonNames[Mouse::kButtonCount];
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

const char* Gamepad::s_buttonNames[Gamepad::kButtonCount];
void Gamepad::InitButtonNames()
{
	SET_BUTTON_NAME(UnmappedButton);
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
