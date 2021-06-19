#pragma once

#include <frm/core/frm.h>
#include <frm/core/StringHash.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Serializable
// Base class for serializable objects.
//
// \todo
// - Undo/redo system will be of the form:
//		BeginEdit(ID _id, Serializable& _object);
////////////////////////////////////////////////////////////////////////////////
template <typename tType>
class Serializable
{
	static const char*      kClassName;
	static const StringHash kClassNameHash;
	static const int        kClassVersion;

public:
	
	static const char* GetClassName()                                                                 { return kClassName; }
	static StringHash  GetClassNameHash()                                                             { return kClassNameHash; }
	static int         GetClassVersion()                                                              { return kClassVersion; }

	bool               serialize(Serializer& _serializer_)                                            { return tType::serialize(_serializer_); }

protected:

	static bool        SerializeAndValidateClassName(Serializer& _serializer_);
	static bool        SerializeAndValidateClassVersion(Serializer& _serializer_, int* version_);
	static bool        SerializeAndValidateClass(Serializer& _serializer_, int* version_ = nullptr)   { return SerializeAndValidateClassName(_serializer_) && SerializeAndValidateClassVersion(_serializer_, version_); }
};

#define FRM_SERIALIZABLE_DEFINE(_class, _version) \
	const char* frm::Serializable<_class>::kClassName = #_class; \
	const frm::StringHash frm::Serializable<_class>::kClassNameHash = frm::StringHash(#_class); \
	const int frm::Serializable<_class>::kClassVersion = _version

} // namespace frm
