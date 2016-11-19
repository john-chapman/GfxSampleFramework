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
	class Buffer;
	class Camera;
	class Framebuffer;
	class GlContext;
	class Mesh;
	class MeshBuilder;
	class MeshData;
	class MeshDesc;
	class Node;
	class Serializer;
	class Scene;
	class Shader;
	class Texture;
	class TextureAtlas;
	class Window;

	struct AlignedBox;
	struct Capsule;
	struct Cylinder;
	struct Line;
	struct Ray;
	struct Sphere;	
	struct Plane;
}

#endif // frm_def_h
