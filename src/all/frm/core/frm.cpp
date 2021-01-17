#include <frm/core/frm.h>

#include <frm/core/log.h>
#include <frm/core/String.h>

#include <cstdarg> // va_list, va_start, va_end

namespace frm {

static thread_local AssertCallback* g_AssertCallback = &DefaultAssertCallback;

void SetAssertCallback(AssertCallback* _callback) 
{
	g_AssertCallback = _callback;
}

AssertCallback* GetAssertCallback() 
{
	return g_AssertCallback;
}

AssertBehavior DefaultAssertCallback(const char* _expr, const char* _msg, const char* _file, int _line) 
{
	FRM_LOG_ERR("FRM_ASSERT (%s, line %d)\n\t'%s' %s", _file, _line, _expr ? _expr : "", _msg ? _msg : "");
	return AssertBehavior_Break;
}

AssertBehavior internal::AssertAndCallback(const char* _expr, const char* _file, int _line, const char* _msg, ...) 
{
	thread_local String<1024> msg = "";

	if (_msg != 0) 
	{
		va_list args;
		va_start(args, _msg);
		msg.setfv(_msg, args);
		va_end(args);
	}

	if (g_AssertCallback) 
	{
		return g_AssertCallback(_expr, (const char*)msg, internal::StripPath(_file), _line);
	}
	return AssertBehavior_Break;
}

const char* internal::StripPath(const char* _path) 
{
	int i = 0, last = 0;
	while (_path[i] != 0) 
	{
		if (_path[i] == '\\' || _path[i] == '/')
		{
			last = i + 1;
		}
		++i;
	}
	return &_path[last];
}

// See note in frm.h
FRM_FORCE_LINK(BasicRenderableComponent);
FRM_FORCE_LINK(BasicLightComponent);
FRM_FORCE_LINK(ImageLightComponent);
FRM_FORCE_LINK(LookAtComponent);
FRM_FORCE_LINK(FreeLookComponent);
FRM_FORCE_LINK(OrbitLookComponent);
FRM_FORCE_LINK(TextComponent);
FRM_FORCE_LINK(XFormComponent);
FRM_FORCE_LINK(XFormSpin);
FRM_FORCE_LINK(XFormPositionTarget);

#if FRM_MODULE_PHYSICS
	FRM_FORCE_LINK(PhysicsComponent);
	FRM_FORCE_LINK(CharacterControllerComponent);
#endif

} // namespace frm
