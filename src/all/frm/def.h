#pragma once
#ifndef frm_def_h
#define frm_def_h

#include <apt/apt.h>
#include <apt/types.h>

namespace frm {
	using apt::sint8;
	using apt::uint8;
	using apt::sint16;
	using apt::uint16;
	using apt::sint32;
	using apt::uint32;
	using apt::sint64;
	using apt::uint64;
	using apt::sint8N;
	using apt::uint8N;
	using apt::sint16N;
	using apt::uint16N;
	using apt::sint32N;
	using apt::uint32N;
	using apt::sint64N;
	using apt::uint64N;
	using apt::sint;
	using apt::uint;
	using apt::float32;
	using apt::float16;
	using apt::float64;

 // forward declarations
	class  App;
	class  AppSample;
	class  AppSample3d;
	class  Buffer;
	class  Camera;
	class  Curve;
	class  CurveEditor;
	class  Device;
	class  Framebuffer;
	class  Gamepad;
	class  GlContext;
	class  Keyboard;
	class  Light;
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
	class  Scene;
	class  Shader;
	class  ShaderDesc;
	class  Skeleton;
	class  SkeletonAnimation;
	class  SkeletonAnimationTrack;
	class  SplinePath;
	class  Texture;
	class  TextureAtlas;
	struct TextureView;
	class  ValueCurve;
	class  ValueCurveEditor;
	class  VertexAttr;
	class  VirtualWindow;
	class  Window;
	class  XForm;

	struct AlignedBox;
	struct Capsule;
	struct Cylinder;
	struct Frustum;
	struct Line;
	struct LineSegment;
	struct Plane;
	struct Ray;
	struct Sphere;
}

// Config

// Control Z range in NDC, either FRM_NDC_Z_NEG_ONE_TO_ONE (OpenGL default) or FRM_NDC_Z_ZERO_TO_ONE (Direct3D default).
// This modifies how the projection matrix is constructed, see Camera.cpp.
#if !defined(FRM_NDC_Z_ZERO_TO_ONE) && !defined(FRM_NDC_Z_NEG_ONE_TO_ONE)
	#define FRM_NDC_Z_ZERO_TO_ONE 1
#endif

#endif // frm_def_h
