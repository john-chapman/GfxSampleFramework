#include <frm/GlContext.h>

#include <frm/def.h>
#include <frm/Window.h>

#include <apt/log.h>
#include <apt/platform.h>
#include <apt/win.h>

#include <GL/wglew.h>

using namespace frm;


static APT_THREAD_LOCAL GlContext* g_currentCtx = 0;
static APT_THREAD_LOCAL PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormat = 0;
static APT_THREAD_LOCAL PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribs = 0;


/*******************************************************************************

                               GlContext::Impl

*******************************************************************************/

struct GlContext::Impl
{
	HDC   m_hdc;
	HGLRC m_hglrc;
	HWND  m_hwnd; // copy of the associated window's handle
};


/*******************************************************************************

                                 GlContext

*******************************************************************************/

// PUBLIC

GlContext* GlContext::Create(const Window* _window, int _vmaj, int _vmin, bool _compatibility)
{
	GlContext* ret = new GlContext;
	APT_ASSERT(ret);
	ret->m_window = _window;

	GlContext::Impl* impl = ret->m_impl = new GlContext::Impl;
	APT_ASSERT(impl);
	impl->m_hwnd = (HWND)_window->getHandle();

	// create dummy context for extension loading
	static ATOM wndclassex = 0;
	if (wndclassex == 0) {
		WNDCLASSEX wc;
		memset(&wc, 0, sizeof(wc));
		wc.cbSize = sizeof(wc);
		wc.style = CS_OWNDC;// | CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = DefWindowProc;
		wc.hInstance = GetModuleHandle(0);
		wc.lpszClassName = "opengl_context_window";
		wc.hCursor = LoadCursor(0, IDC_ARROW);
		APT_PLATFORM_VERIFY(wndclassex = RegisterClassEx(&wc));
	}
	HWND hwndDummy = CreateWindowEx(0, MAKEINTATOM(wndclassex), 0, NULL, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(0), NULL);
	APT_PLATFORM_ASSERT(hwndDummy);
	HDC hdcDummy = 0;
	APT_PLATFORM_VERIFY(hdcDummy = GetDC(hwndDummy));	
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(pfd));
	APT_PLATFORM_VERIFY(SetPixelFormat(hdcDummy, 1, &pfd));
	HGLRC hglrcDummy = 0;
	APT_PLATFORM_VERIFY(hglrcDummy = wglCreateContext(hdcDummy));
	APT_PLATFORM_VERIFY(wglMakeCurrent(hdcDummy, hglrcDummy));
	
 // check the platform supports the requested GL version
	GLint platformVMaj, platformVMin;
	glAssert(glGetIntegerv(GL_MAJOR_VERSION, &platformVMaj));
	glAssert(glGetIntegerv(GL_MINOR_VERSION, &platformVMin));
	if (platformVMaj < _vmaj || (platformVMaj >= _vmaj && platformVMin < _vmin)) {
		APT_LOG_ERR("OpenGL version %d.%d is not available (max version is %d.%d)", _vmaj, _vmin, platformVMaj, platformVMin);
		APT_LOG("This error may occur if the platform has an integrated GPU");
		APT_ASSERT(false);
		return 0;
	}
	

 // load wgl extensions for true context creation
	APT_PLATFORM_VERIFY(wglChoosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB"));
	APT_PLATFORM_VERIFY(wglCreateContextAttribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB"));

 // delete the dummy context
	APT_PLATFORM_VERIFY(wglMakeCurrent(0, 0));
	APT_PLATFORM_VERIFY(wglDeleteContext(hglrcDummy));
	APT_PLATFORM_VERIFY(ReleaseDC(hwndDummy, hdcDummy) != 0);
	APT_PLATFORM_VERIFY(DestroyWindow(hwndDummy) != 0);

 // create true context
	APT_PLATFORM_VERIFY(impl->m_hdc = GetDC(impl->m_hwnd));
	const int pfattr[] = {
		WGL_SUPPORT_OPENGL_ARB, 1,
		WGL_DRAW_TO_WINDOW_ARB, 1,
		WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
		WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
		WGL_DOUBLE_BUFFER_ARB,  1,
		WGL_COLOR_BITS_ARB,     24,
		WGL_ALPHA_BITS_ARB,     8,
		WGL_DEPTH_BITS_ARB,     0,
		WGL_STENCIL_BITS_ARB,   0,
		0
	};
    int pformat = -1, npformat = -1;
	APT_PLATFORM_VERIFY(wglChoosePixelFormat(impl->m_hdc, pfattr, 0, 1, &pformat, (::UINT*)&npformat));
	APT_PLATFORM_VERIFY(SetPixelFormat(impl->m_hdc, pformat, &pfd));
	int profileBit = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
	if (_compatibility) {
		profileBit = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
	}
	int attr[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB,	_vmaj,
		WGL_CONTEXT_MINOR_VERSION_ARB,	_vmin,
		WGL_CONTEXT_PROFILE_MASK_ARB,	profileBit,
		0
	};
	APT_PLATFORM_VERIFY(impl->m_hglrc = wglCreateContextAttribs(impl->m_hdc, 0, attr));

 // load extensions
	MakeCurrent(ret);
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	APT_ASSERT(err == GLEW_OK);
	glGetError(); // clear any errors caused by glewInit()

	ret->setVsyncMode(ret->getVsyncMode());
	ret->queryLimits();

	APT_LOG("OpenGL context:\n\tVersion: %s\n\tGLSL Version: %s\n\tVendor: %s\n\tRenderer: %s",
		internal::GlGetString(GL_VERSION),
		internal::GlGetString(GL_SHADING_LANGUAGE_VERSION),
		internal::GlGetString(GL_VENDOR),
		internal::GlGetString(GL_RENDERER)
		);
	return ret;
}

void GlContext::Destroy(GlContext*& _ctx_)
{
	APT_ASSERT(_ctx_ != 0);
	APT_ASSERT(_ctx_->m_impl != 0);

	APT_PLATFORM_VERIFY(wglMakeCurrent(0, 0));
	APT_PLATFORM_VERIFY(wglDeleteContext(_ctx_->m_impl->m_hglrc));
	APT_PLATFORM_VERIFY(ReleaseDC(_ctx_->m_impl->m_hwnd, _ctx_->m_impl->m_hdc) != 0);
		
	delete _ctx_->m_impl;
	_ctx_->m_impl = 0;
	delete _ctx_;
	_ctx_ = 0;
}

GlContext* GlContext::GetCurrent()
{
	return g_currentCtx;
}

bool GlContext::MakeCurrent(GlContext* _ctx)
{
	APT_ASSERT(_ctx != 0);

	if (_ctx != g_currentCtx) {
		if (!wglMakeCurrent(_ctx->m_impl->m_hdc, _ctx->m_impl->m_hglrc)) {
			return false;
		}
		g_currentCtx = _ctx;
	}
	return true;
}

void GlContext::present()
{
	APT_PLATFORM_VERIFY(SwapBuffers(m_impl->m_hdc));
	APT_PLATFORM_VERIFY(ValidateRect(m_impl->m_hwnd, 0)); // suppress WM_PAINT
	++m_frameIndex;
}

void GlContext::setVsyncMode(VsyncMode _mode)
{
	if (m_vsyncMode	!= _mode) {
		m_vsyncMode = _mode;
		APT_PLATFORM_VERIFY(wglSwapIntervalEXT((int)_mode));
	}
}
