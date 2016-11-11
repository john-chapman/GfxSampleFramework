#pragma once
#ifndef frm_XForm_h
#define frm_XForm_h

#include <frm/def.h>
#include <frm/math.h>

#include <frm/Input.h>
#include <frm/Scene.h>

#include <apt/log.h>
#include <apt/String.h>
#include <apt/StringHash.h>

#include <vector>

namespace frm
{

class Node;

////////////////////////////////////////////////////////////////////////////////
/// \class XForm
////////////////////////////////////////////////////////////////////////////////
class XForm
{
public:

	virtual const char* getName() const                = 0;
	virtual void        apply(Node* _node_, float _dt) = 0;	
	virtual void        edit()                         = 0;



	static XForm* Create(apt::StringHash _nameHash);
	static void Destroy(XForm*& _xform_);
	
	struct ClassRef
	{
		const char*      m_name;
		apt::StringHash  m_nameHash;

		ClassRef(const char* _name)
			: m_name(_name)
			, m_nameHash(_name)
		{
			s_registry.push_back(this);
		}

		virtual XForm* create() = 0;
	};

	
	static int GetClassRefCount() { return (int)s_registry.size(); }
	static ClassRef* GetClassRef(int _i) { return s_registry[_i]; }

	template <typename tSubClass>
	struct AutoRegister: public ClassRef
	{
		AutoRegister(): ClassRef(tSubClass::GetName()) {}
		virtual XForm* create() override { return new tSubClass(); }
	};

private:
	static std::vector<ClassRef*> s_registry;

}; // class XForm


struct XForm_PositionOrientationScale: public XForm
{
	vec3  m_position;
	quat  m_orientation;
	vec3  m_scale;

	static const char* GetName() { return "PositionOrientationScale"; }

	XForm_PositionOrientationScale();
	virtual ~XForm_PositionOrientationScale() {}

	virtual const char* getName() const override { return GetName(); }
	virtual void        apply(Node* _node_, float _dt) override;
	virtual void        edit() override;
};

struct XForm_FreeCamera: public XForm
{
	vec3  m_position;
	vec3  m_velocity;
	float m_speed;
	float m_maxSpeed;
	float m_maxSpeedMul;        //< Multiplies m_speed for speed 'boost'.
	float m_accelTime;          //< Acceleration ramp length in seconds.
	float m_accelCount;         //< Current ramp position in [0,m_accelTime].

	quat  m_orientation;
	vec3  m_pitchYawRoll;       //< Angular velocit in rads/sec.
	float m_rotationInputMul;   //< Scale rotation inputs (should be relative to fov/screen size).
	float m_rotationDamp;       //< Adhoc damping factor.

	static const char* GetName() { return "FreeCamera"; }

	XForm_FreeCamera();
	virtual ~XForm_FreeCamera() {}

	virtual const char* getName() const override { return GetName(); }
	virtual void        apply(Node* _node_, float _dt) override;
	virtual void        edit() override;
};

} // namespace frm

#endif // frm_XForm_h
