#include <frm/Window.h>

#include <cstring>

#include <vector>

using namespace frm;

static std::vector<Window*> g_instances;

// PUBLIC

Window::Callbacks::Callbacks()
{
	memset(this, 0, sizeof(Callbacks));
}

Window* Window::Find(const void* _handle)
{
	APT_ASSERT(_handle != 0);
	for (auto it = g_instances.begin(); it != g_instances.end(); ++it) {
		if ((*it)->getHandle() == _handle) {
			return *it;
		}
	}
	return 0;
}


// PRIVATE

Window::Window()
	: m_width(-1), m_height(-1)
	, m_title("")
	, m_impl(0)
{
	g_instances.push_back(this);
}

Window::~Window()
{
	APT_ASSERT(m_impl == 0);
	APT_ASSERT(m_handle == 0);
	for (auto it = g_instances.begin(); it != g_instances.end(); ++it) {
		if (*it == this) {
			g_instances.erase(it);
			break;
		}
	}
}
