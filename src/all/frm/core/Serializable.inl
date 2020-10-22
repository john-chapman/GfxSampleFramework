#pragma once

#include "Serializable.h"

#include <frm/core/log.h>
#include <frm/core/Serializer.h>

namespace frm {

template <typename tType>
inline bool Serializable<tType>::SerializeAndValidateClassName(Serializer& _serializer_)
{
	String<32> className = kClassName;
	if (_serializer_.getMode() == Serializer::Mode_Read) 
	{
		if (!_serializer_.value(className, "_class")) 
		{
			FRM_LOG_ERR("Failed to serialize _class (%s)", kClassName);
			return false;
		}

		if (className != kClassName) 
		{
			FRM_LOG_ERR("Invalid _class; expected '%s' but found '%s'", kClassName, className.c_str());
			return false;
		}

		return true;

	}
	else 
	{
		return _serializer_.value(className, "_class");
	}
}

template <typename tType>
inline bool Serializable<tType>::SerializeAndValidateClassVersion(Serializer& _serializer_)
{
	int classVersion = kClassVersion;
	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		if (!_serializer_.value(classVersion, "_version")) 
		{
			FRM_LOG_ERR("Failed to serialize _version (%s)", kClassName);
			return false;
		}
		
		if (classVersion > kClassVersion) // \todo optional backward compatibility?
		{
			FRM_LOG_ERR("Invalue _version; expected '%d' but found '%d' (%s)", kClassVersion, classVersion, kClassName);
			return false;
		}
		
		return true;

	}
	else 
	{
		return _serializer_.value(classVersion, "_version");
	}
}

} // namespace frm
