#pragma once
#ifndef frm_def_h
#define frm_def_h

#include <apt/def.h>
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
	using apt::float64;

	using apt::DataType;

 // forward declarations
	class  App;
	class  AppProperty;
	class  AppPropertyGroup;
	class  AppProperties;
	class  AppSample;
	class  AppSample3d;
	class  Buffer;
	class  Camera;
	class  Device;
	class  Framebuffer;
	class  Gamepad;
	class  GlContext;
	class  Keyboard;
	class  LuaScript;
	class  Mesh;
	class  MeshBuilder;
	class  MeshData;
	class  MeshDesc;
	class  Mouse;
	class  Node;
	class  ProxyDevice;
	class  ProxyGamepad;
	class  ProxyKeyboard;
	class  ProxyMouse;
	class  Serializer;
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

#endif // frm_def_h
