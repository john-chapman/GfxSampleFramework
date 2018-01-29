#include <frm/GlContext.h>

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/Profiler.h>
#include <frm/Window.h>

#include <apt/log.h>
#include <apt/platform.h>
#include <apt/win.h>

#include <GL/wglew.h>

using namespace frm;

// force Nvidia/AMD drivers to use the discrete GPU
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static APT_THREAD_LOCAL GlContext* g_currentCtx;


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
	APT_PLATFORM_VERIFY(impl->m_hdc = GetDC(impl->m_hwnd));

 // set the window pixel format
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion     = 1;
	pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_GENERIC_ACCELERATED;
	pfd.iPixelType   = PFD_TYPE_RGBA;
	pfd.cColorBits   = 24;
	pfd.cDepthBits   = 24;
	pfd.dwDamageMask = 8;
	int pformat = 0;
	APT_PLATFORM_VERIFY(pformat = ChoosePixelFormat(GetDC(impl->m_hwnd), &pfd));
	APT_PLATFORM_VERIFY(SetPixelFormat(impl->m_hdc, pformat, &pfd));
	
 // create dummy context to load wgl extensions
	HGLRC hGLRC = 0;
	APT_PLATFORM_VERIFY(hGLRC = wglCreateContext(impl->m_hdc));
	APT_PLATFORM_VERIFY(wglMakeCurrent(impl->m_hdc, hGLRC));

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
	static APT_THREAD_LOCAL PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribs;
	APT_PLATFORM_VERIFY(wglCreateContextAttribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB"));

 // delete the dummy context
	APT_PLATFORM_VERIFY(wglMakeCurrent(0, 0));
	APT_PLATFORM_VERIFY(wglDeleteContext(hGLRC));

 // create true context
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

	APT_LOG("OpenGL context:\n\tVersion: %s\n\tGLSL Version: %s\n\tVendor: %s\n\tRenderer: %s",
		internal::GlGetString(GL_VERSION),
		internal::GlGetString(GL_SHADING_LANGUAGE_VERSION),
		internal::GlGetString(GL_VENDOR),
		internal::GlGetString(GL_RENDERER)
		);

 // set default states
	#if FRM_NDC_Z_ZERO_TO_ONE
		APT_ASSERT(glewIsExtensionSupported("GL_ARB_clip_control"));
		glAssert(glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE));
	#endif
	APT_ASSERT(glewIsExtensionSupported("GL_ARB_seamless_cube_map"));
	glAssert(glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS));

	APT_VERIFY(ret->init());

	return ret;
}

void GlContext::Destroy(GlContext*& _ctx_)
{
	APT_ASSERT(_ctx_ != 0);
	APT_ASSERT(_ctx_->m_impl != 0);

	_ctx_->shutdown();

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
	PROFILER_VALUE_CPU("#Draw Calls", m_drawCount);
	PROFILER_VALUE_CPU("#Dispatch", m_dispatchCount);
	m_dispatchCount = m_drawCount = 0;
}

void GlContext::setVsync(Vsync _mode)
{
	if (m_vsync	!= _mode) {
		m_vsync = _mode;
		APT_PLATFORM_VERIFY(wglSwapIntervalEXT((int)_mode));
	}
}
