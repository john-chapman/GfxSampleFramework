#pragma once

#include <frm/core/def.h>
#include <apt/String.h>
#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Window
////////////////////////////////////////////////////////////////////////////////
class Window: private apt::non_copyable<Window>
{
public:
	// If _width or _height is -1, the windows size is set to the size of the primary display.
	static Window* Create(int _width, int _height, const char* _title);
	static void    Destroy(Window*& _window_);
	static Window* Find(const void* _handle);

	// Poll window events, dispatch to callbacks.
	// Return true if the application should continue (i.e. if no quit message was received).
	bool pollEvents(); // non-blocking
	bool waitEvents(); // blocking

	void show() const;
	void hide() const;
	void maximize() const;
	void minimize() const;
	void setPositionSize(int _x, int _y, int _width, int _height);

	bool hasFocus()    const;
	bool isMinimized() const;
	bool isMaximized() const;

	void getWindowRelativeCursor(int* x_, int* y_) const;
	
	// Callbacks should return true if the event was consumed.
	typedef bool (OnShow)       (Window* _window);
	typedef bool (OnHide)       (Window* _window);
	typedef bool (OnResize)     (Window* _window, int _width, int _height);
	
	typedef bool (OnChar)       (Window* _window, char _key);
	typedef bool (OnKey)        (Window* _window, unsigned _key, bool _isDown);    // _key is a Keyboard::Button

	typedef bool (OnMouseButton)(Window* _window, unsigned _button, bool _isDown); // _key is a Mouse::Button
	typedef bool (OnMouseWheel) (Window* _window, float _delta);

	typedef bool (OnFileDrop)   (Window* _window, const char* _path);              // _path is only valid during the callback

	struct Callbacks
	{
		OnShow*        m_OnShow           = nullptr;
		OnHide*        m_OnHide           = nullptr;
		OnResize*      m_OnResize         = nullptr;

		OnKey*         m_OnKey            = nullptr;
		OnChar*        m_OnChar           = nullptr;

		OnMouseButton* m_OnMouseButton    = nullptr;
		OnMouseWheel*  m_OnMouseWheel     = nullptr;

		OnFileDrop*    m_OnFileDrop       = nullptr;
	};

	void setCallbacks(const Callbacks& _callbacks) { m_callbacks = _callbacks; }
	const Callbacks& getCallbacks() const          { return m_callbacks; }
	
	// Return a list of files dropped onto the window during this frame.
	// It is may be useful to call this function instead of using the OnFileDrop callback in cases where the application needs
	// to check whether an internally rendered control is focused/hovered.
	typedef eastl::vector<apt::String<64> > FileList;
	FileList    getFileDropList() const            { return m_fileDropList; }

	// Return the UI scaling factor. Note that this may change if the window moves between monitors. 
	float       getScaling() const;

	int         getWidth()  const                  { return m_width;  }
	int         getHeight() const                  { return m_height; }
	void*       getHandle() const                  { return m_handle; }
	const char* getTitle()  const                  { return m_title;  }

private:
	void*       m_handle        = nullptr;
	int         m_width         = -1;
	int         m_height        = -1;
	const char* m_title         = "";
	Callbacks   m_callbacks;
	FileList    m_fileDropList;
	
	struct Impl;
	Impl* m_impl                = nullptr;

	Window();
	~Window();

}; // class Window

} // namespace frm
