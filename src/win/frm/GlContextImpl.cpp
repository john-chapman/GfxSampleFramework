#include <frm/GlContext.h>

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>
#include <frm/Window.h>

#include <apt/log.h>
#include <apt/platform.h>
#include <apt/win.h>
#include <apt/String.h>

#include <GL/wglew.h>

using namespace frm;

// force Nvidia/AMD drivers to use the discrete GPU
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static APT_THREAD_LOCAL GlContext* g_currentCtx;

static void GlDebugMessageCallback(GLenum _source, GLenum _type, GLuint _id, GLenum _severity, GLsizei _length, const GLchar* _message, const void* _user)
{
 // filter messages
	if (_severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
		return;
	}

 // log
	apt::String<512> msg("[%s] [%s] [%s] (%u): ", internal::GlEnumStr(_source), internal::GlEnumStr(_type), internal::GlEnumStr(_severity), _id);
	msg.append(_message, _length);
	switch (_type) {
		case GL_DEBUG_TYPE_ERROR:
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			APT_LOG_ERR((const char*)msg);
			break;
		default:
			APT_LOG((const char*)msg);
	};
}

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

GlContext* GlContext::Create(const Window* _window, int _vmaj, int _vmin, CreateFlags _flags)
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
	_vmaj = _vmaj < 0 ? platformVMaj : _vmaj;
	_vmin = _vmin < 0 ? platformVMin : _vmin;
	if (platformVMaj < _vmaj || (platformVMaj >= _vmaj && platformVMin < _vmin)) {
		APT_LOG_ERR("OpenGL version %d.%d is not available (available version is %d.%d).", _vmaj, _vmin, platformVMaj, platformVMin);
		APT_LOG("This error may occur if the platform has an integrated GPU.");
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
	int profileMask = ((_flags & CreateFlags_Compatibility) != 0) ? WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB : WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
	int ctxFlags    = ((_flags & CreateFlags_Debug) != 0) ? WGL_CONTEXT_DEBUG_BIT_ARB : 0;
	int attr[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB,	_vmaj,
		WGL_CONTEXT_MINOR_VERSION_ARB,	_vmin,
		WGL_CONTEXT_PROFILE_MASK_ARB,	profileMask,
		WGL_CONTEXT_FLAGS_ARB,          ctxFlags,
		0
	};
	APT_PLATFORM_VERIFY(impl->m_hglrc = wglCreateContextAttribs(impl->m_hdc, 0, attr));

 // load extensions
	MakeCurrent(ret);
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	APT_ASSERT(err == GLEW_OK);
	glGetError(); // clear any errors caused by glewInit()

	APT_LOG("OpenGL %s%scontext:"
		"\n\tVersion:      %s"
		"\n\tGLSL Version: %s"
		"\n\tVendor:       %s"
		"\n\tRenderer:     %s",
		((_flags & CreateFlags_Compatibility) != 0) ? "compatibility " : "",
		((_flags & CreateFlags_Debug)         != 0) ? "debug " : "",
		internal::GlGetString(GL_VERSION),
		internal::GlGetString(GL_SHADING_LANGUAGE_VERSION),
		internal::GlGetString(GL_VENDOR),
		internal::GlGetString(GL_RENDERER)
		);
	if ((_flags & CreateFlags_Debug) != 0) {
		APT_ASSERT(glewIsExtensionSupported("GL_ARB_debug_output"));
		glAssert(glDebugMessageCallback(GlDebugMessageCallback, 0));
		glAssert(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
		glAssert(glEnable(GL_DEBUG_OUTPUT));
	}

 // set default states
	ShaderDesc::SetDefaultVersion((const char*)apt::String<8>("%d%d0", _vmaj, _vmin));
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
	PROFILER_VALUE_CPU("#Draw Call Count", m_drawCount,     "%.0f");
	PROFILER_VALUE_CPU("#Dispatch Count",  m_dispatchCount, "%.0f");
	m_dispatchCount = m_drawCount = 0;
}

void GlContext::setVsync(Vsync _mode)
{
	if (m_vsync	!= _mode) {
		m_vsync = _mode;
		APT_PLATFORM_VERIFY(wglSwapIntervalEXT((int)_mode));
	}
}
