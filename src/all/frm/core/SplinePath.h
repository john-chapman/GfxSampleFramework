#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/Resource.h>
#include <frm/core/String.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// SplinePath
////////////////////////////////////////////////////////////////////////////////
class SplinePath: public Resource<SplinePath>
{
public:

	static SplinePath*  Create(const char* _path);
	static SplinePath*  Create(Serializer& _serializer_);
	static SplinePath*  CreateUnique();
	static void         Destroy(SplinePath*& _splinePath_);
	
	static bool         Edit(SplinePath*& _splinePath_, bool* _open_);

	// Sample the spline at _t (in [0,1]). _hint_ is useful in the common 
	// case where evaluate() is called repeatedly with a monotonically increasing
	// _t, it avoids performing a binary search on the spline data.
	vec3                samplePosition(float _t, int* _hint_ = nullptr);

	// Append a control point to the spline.
	void                append(const vec3& _position);

	bool                load()            { return reload(); }
	bool                reload();
	bool                edit();
	bool                serialize(Serializer& _serializer_);
	void                draw() const;

	float               getLength() const { return m_length; }
	const char*         getPath() const   { return m_path.c_str(); }

private:

	                    SplinePath(uint64 _id, const char* _name);
	                    ~SplinePath();
							
	// Construct derived members (evaluation metadata, spline length).
	void                build();
		
	// Recursively subdivide a segment from m_raw, populating
	void                subdiv(int _segment, float _t0 = 0.0f, float _t1 = 1.0f, float _maxError = 1e-6f, int _limit = 5);

	// Find the segment containing _t. Implicitly calls build() if m_eval is empty.
	int                 findSegment(float _t, int* _hint_);

	PathStr             m_path   = "";   // Empty if not from a file
	float               m_length = 0.0f; // Total spline length.
	eastl::vector<vec3> m_raw;           // Raw control points (for edit/serialize).
	eastl::vector<vec4> m_eval;          // Subdivided spline (for evaluation). xyz = position, w = normalized segment start.
};

} // namespace frm
