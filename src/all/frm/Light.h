#pragma once
#ifndef frm_Light_h
#define frm_Light_h

#include <frm/def.h>
#include <frm/math.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Light
////////////////////////////////////////////////////////////////////////////////
class Light
{
public:
	enum Type
	{
		Type_Direct,

		Type_Count
	};

	Light(Node* _parent = nullptr);
	~Light();
	
	/*Light(const Light& _rhs);
	Light(Light&& _rhs);
	Light& operator=(Light&& _rhs);
	friend void swap(Light& _a_, Light& _b_);*/

	friend bool Serialize(apt::Serializer& _serializer_, Light& _light_);
	void edit();
	
	Node*   m_parent;
	
private:

}; // class Light

} // namespace frm

#endif // frm_Light_h
