#pragma once

#define FRM_VERSION "0.31"

#include <frm/core/config.h>

#if FRM_COMPILER_GNU
	#define if_likely(e)   if ( __builtin_expect(!!(e), 1) )
	#define if_unlikely(e) if ( __builtin_expect(!!(e), 0) )
//#elif defined(FRM_COMPILER_MSVC)
  // not defined for MSVC
#else
	#define if_likely(e)   if(!!(e))
	#define if_unlikely(e) if(!!(e))
#endif

#ifndef __COUNTER__
// __COUNTER__ is non-standard, so replace with __LINE__ if it's unavailable
	#define __COUNTER__ __LINE__
#endif
#define FRM_TOKEN_CONCATENATE_(_t0, _t1) _t0 ## _t1
#define FRM_TOKEN_CONCATENATE(_t0, _t1)  FRM_TOKEN_CONCATENATE_(_t0, _t1)
#define FRM_UNIQUE_NAME(_base) FRM_TOKEN_CONCATENATE(_base, __COUNTER__)

#define FRM_ARRAY_COUNT(_arr) frm::internal::ArrayCount(_arr)

// Execute a code block once, e.g. FRM_ONCE { foo; bar; }
// This is not thread safe.
#define FRM_ONCE  for (static bool _done = false; _done ? false : _done = true; )

namespace frm {

enum AssertBehavior
{
	AssertBehavior_Break, 
	AssertBehavior_Continue
};

// Typedef for assert callbacks.
typedef AssertBehavior (AssertCallback)(const char* _expr, const char* _msg, const char* _file, int _line);

// Return the current assert callback. Default is DefaultAssertCallback.
AssertCallback* GetAssertCallback();

// Set the function to be called when asserts fail, which may be 0.
void SetAssertCallback(AssertCallback* _callback);

// Default assert callback, print message via FRM_LOG_ERR(). Always returns AssertBehavior_Break.
AssertBehavior DefaultAssertCallback(const char* _expr, const char* _msg,  const char* _file,  int _line);

} // namespace frm


namespace frm { namespace internal {

AssertBehavior AssertAndCallback(const char* _expr, const char* _file, int _line, const char* _msg, ...);
const char* StripPath(const char* _path);

template <typename tType, unsigned kCount>
inline constexpr unsigned ArrayCount(const tType (&)[kCount]) { return kCount; }

} } // namespace frm::internal

#define FRM_UNUSED(x) do { (void)sizeof(x); } while(0)
#if FRM_ENABLE_ASSERT
	#if FRM_COMPILER_MSVC
		#define FRM_BREAK() __debugbreak()
	#else
		#include <cstdlib>
		#define FRM_BREAK() abort()
	#endif

	#define FRM_ASSERT_MSG(e, msg, ...) \
		do { \
			if_unlikely (!(e)) { \
				if (frm::internal::AssertAndCallback(#e, __FILE__, __LINE__, msg, ## __VA_ARGS__) == frm::AssertBehavior_Break) \
				{ FRM_BREAK(); } \
			} \
		} while(0)

	#define FRM_ASSERT(e)                 FRM_ASSERT_MSG(e, 0, 0)
	#define FRM_VERIFY_MSG(e, msg, ...)   FRM_ASSERT_MSG(e, msg, ## __VA_ARGS__)
	#define FRM_VERIFY(e)                 FRM_VERIFY_MSG(e, 0, 0)

#else
	#define FRM_BREAK()
	#define FRM_ASSERT_MSG(e, msg, ...)   do { FRM_UNUSED(e); FRM_UNUSED(msg); } while(0)
	#define FRM_ASSERT(e)                 do { FRM_UNUSED(e); } while(0)
	#define FRM_VERIFY_MSG(e, msg, ...)   do { (void)(e); FRM_UNUSED(msg); } while(0)
	#define FRM_VERIFY(e)                 (void)(e)

#endif // FRM_ENABLE_ASSERT

#if FRM_ENABLE_STRICT_ASSERT
	#define FRM_STRICT_ASSERT(e) FRM_ASSERT(e)
#else
	#define FRM_STRICT_ASSERT(e) do { FRM_UNUSED(e); } while(0)
#endif

#define FRM_STATIC_ASSERT(e) { (void)sizeof(char[(e) ? 1 : -1]); }

#include <frm/core/types.h>

namespace frm {
 // forward declarations
	class  App;
	class  AppSample;
	class  AppSample3d;
	class  BasicMaterial;
	struct BasicRenderer;
	class  Buffer;
	class  Camera;
	class  Component;
		struct Component_BasicRenderable;
		struct Component_BasicLight;
		struct Component_EnvironmentLight;
	class  Curve;
	class  CurveEditor;
	class  CurveGradient;
	class  Device;
	class  Framebuffer;
	class  Gamepad;
	class  GlContext;
	class  GradientEditor;
	class  Keyboard;
	class  LuaScript;
	class  Mesh;
	class  MeshBuilder;
	class  MeshData;
	class  MeshDesc;
	class  Mouse;
	class  Node;
	class  Property;
	class  PropertyGroup;
	class  Properties;
	class  ProxyDevice;
	class  ProxyGamepad;
	class  ProxyKeyboard;
	class  ProxyMouse;
	struct RenderTarget;
	class  Scene;
	class  Shader;
	class  ShaderDesc;
	class  ShadowAtlas;
	class  Skeleton;
	class  SkeletonAnimation;
	class  SkeletonAnimationTrack;
	class  SplinePath;
	class  StreamingQuadtree;
	class  Texture;
	class  TextureAtlas;
	class  TextureSampler;
	struct TextureView;
	class  ValueCurve;
	class  ValueCurveEditor;
	class  VertexAttr;
	struct Viewport;
	class  VirtualWindow;
	class  Window;
	class  XForm;

	#if FRM_MODULE_AUDIO
		class Audio;
		class AudioData;
	#endif

	#if FRM_MODULE_PHYSICS
		class  Physics;
		class  PhysicsConstraint;
		class  PhysicsMaterial;
		class  PhysicsGeometry;
		struct Component_Physics;
	#endif

	#if FRM_MODULE_VR
		class  AppSampleVR;
		class  VRContext;
		class  VRController;
		class  VRInput;
	#endif

	struct AlignedBox;
	struct Capsule;
	struct Cylinder;
	struct Frustum;
	struct Line;
	struct LineSegment;
	struct Plane;
	struct Ray;
	struct Sphere;

	class ArgList;
	template <typename tEnum> struct BitFlags;
	template <typename tType> class Factory;
	class File;
	class FileSystem;
	class Image;
	class Json;
	class MemoryPool;
	template <typename tIndex, typename tNode> class Octree;
	template <typename tType> class PersistentVector;
	template <typename tType> class Pool;
	template <typename tIndex, typename tNode> class Quadtree;
	template <typename PRNG>  class Rand;
	template <typename tType> class RingBuffer;
	class Serializer;
		class SerializerJson;
	class StringBase;
		template <uint kCapacity> class String;
	class StringHash;
	class TextParser;
	class Timestamp;
	class DateTime;

	typedef String<128> PathStr;

	////////////////////////////////////////////////////////////////////////////////
	// non_copyable
	// Mixin class, forces a derived class to be non-copyable.
	// The template parameter permits Empty Base Optimization (see
	//   http://en.wikibooks.org/wiki/More_C++_Idioms/Non-copyable_Mixin).
	////////////////////////////////////////////////////////////////////////////////
	template <typename tType>
	class non_copyable
	{
	protected:
		non_copyable() = default;
		~non_copyable() = default;

		non_copyable(const non_copyable&) = delete;
		non_copyable& operator=(const non_copyable&) = delete;
	};

} // namespace frm
