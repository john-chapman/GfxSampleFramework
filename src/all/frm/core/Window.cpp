#include "Window.h"

#include <cstring>

#include <EASTL/vector.h>

using namespace frm;

static eastl::vector<Window*> g_instances;

// PUBLIC

Window* Window::Find(const void* _handle)
{
	APT_STRICT_ASSERT(_handle != nullptr);
	for (auto inst : g_instances) {
		if (inst->getHandle() == _handle) {
			return inst;
		}
	}
	return nullptr;
}


// PRIVATE

Window::Window()
{
	g_instances.push_back(this);
}

Window::~Window()
{
	APT_ASSERT(m_impl == nullptr);
	APT_ASSERT(m_handle == nullptr);
	auto it = eastl::find(g_instances.begin(), g_instances.end(), this);
	g_instances.erase(it);
}
