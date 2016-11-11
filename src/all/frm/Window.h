#pragma once
#ifndef frm_Window_h
#define frm_Window_h

#include <frm/def.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class Window
////////////////////////////////////////////////////////////////////////////////
class Window: private apt::non_copyable<Window>
{
public:
	/// if _width or _height is -1, the windows size is set to the size of the
	/// primary display.
	static Window* Create(int _width, int _height, const char* _title);
	static void Destroy(Window*& _window_);
	static Window* Find(const void* _handle);

	/// Poll window events, dispatch to callbacks. The function is non-blocking.
	/// \return true if the application should continue (i.e. if no quit message
	///   was received.
	bool pollEvents() const;

	/// Poll window events, dispatch to callbacks. The function blocks until an
	/// event occurs.
	/// \return true if the application should continue (i.e. if no quit message
	///   was received.
	bool waitEvents() const;

	void show() const;
	void hide() const;
	void maximize() const;
	void minimize() const;
	void setPositionSize(int _x, int _y, int _width, int _height);

	bool hasFocus() const;
	bool isMinimized() const;
	bool isMaximized() const;

	void getWindowRelativeCursor(int* x_, int* y_) const;
	
	/// Callbacks should return true if the event was consumed.
	typedef bool (OnShow)  (Window* _window);
	typedef bool (OnHide)  (Window* _window);
	typedef bool (OnResize)(Window* _window, int _width, int _height);
	
	typedef bool (OnKey) (Window* _window, unsigned _key, bool _isDown); //< _key is a Keyboard::Button
	typedef bool (OnChar)(Window* _window, char _key);

	typedef bool (OnMouseButton)(Window* _window, unsigned _button, bool _isDown); //< _key is a Mouse::Button
	typedef bool (OnMouseWheel) (Window* _window, float _delta);
	struct Callbacks
	{
		OnShow*        m_OnShow;
		OnHide*        m_OnHide;
		OnResize*      m_OnResize;

		OnKey*         m_OnKey;
		OnChar*        m_OnChar;

		OnMouseButton* m_OnMouseButton;
		OnMouseWheel*  m_OnMouseWheel;

		/// Zero-initialize members.
		Callbacks();
	};

	void setCallbacks(const Callbacks& _callbacks) { m_callbacks = _callbacks; }
	const Callbacks& getCallbacks() const          { return m_callbacks; }

	int   getWidth() const                         { return m_width; }
	int   getHeight() const                        { return m_height; }
	void* getHandle() const                        { return m_handle; }
	const char* getTitle() const                   { return m_title; }

private:
	int m_width, m_height;
	const char* m_title;
	Callbacks m_callbacks;
	void* m_handle;
	
	struct Impl;
	Impl* m_impl;

	Window();
	~Window();

}; // class Window

} // namespace frm

#endif // frm_Window_h
