#pragma once

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// StaticInitializer
// Implementation of the Nifty/Schwarz counter idiom (see
//   https://john-chapman.github.io/2016/09/01/static-initialization.html). 
// Usage:
//
// // in the .h
//    class Foo
//    {
//       FRM_DECALRE_STATIC_INIT_FRIEND(Foo); // give StaticInitializer access to private functions
//       void Init();
//       void Shutdown();
//    };
//    FRM_DECLARE_STATIC_INIT(Foo);
//
// // in the .cpp
//    FRM_DEFINE_STATIC_INIT(Foo, Foo::Init, Foo::Shutdown);
//
// Note that the use of FRM_DECLARE_STATIC_INIT_FRIEND() is optional if the init
// and shutdown methods aren't private members.
//
// Init() should not construct any non-trivial static objects as the order of 
// initialization relative to StaticInitializer cannot be guaranteed. This 
// means that static objects initialized during Init() may subsequently be 
// default-initialized, overwriting the value set by Init(). To get around this, 
// use heap allocation or the storage class (memory.h):
//
//   #inclide <frm/memory.h>
//   static storage<Bar, 1> s_bar;
//
//   void Foo::Init()
//   {
//      new(s_bar) Bar();
//      // ..
////////////////////////////////////////////////////////////////////////////////
template <typename tType>
class StaticInitializer
{
public:
	StaticInitializer()
	{ 
		if_unlikely (++s_initCounter == 1) {
			Init();
		} 
	}
	~StaticInitializer()
	{
		if_unlikely (--s_initCounter == 0) { 
			Shutdown();
		} 
	}
private:
	static int  s_initCounter;

	static void Init();
	static void Shutdown();
};
#define FRM_DECLARE_STATIC_INIT_FRIEND(_type) \
	friend class frm::StaticInitializer<_type>
#define FRM_DECLARE_STATIC_INIT(_type) \
	static frm::StaticInitializer<_type> _type ## _StaticInitializer
#define FRM_DEFINE_STATIC_INIT(_type, _onInit, _onShutdown) \
	template <> int  frm::StaticInitializer<_type>::s_initCounter; \
	template <> void frm::StaticInitializer<_type>::Init()     { _onInit(); } \
	template <> void frm::StaticInitializer<_type>::Shutdown() { _onShutdown(); }

} // namespace frm
