#pragma once

#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
//#define TINYGLTF_NO_FS
#include <tiny_gltf.h>

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/types.h>

namespace tinygltf {

bool Load(const char* _srcData, size_t _srcDataSize, const char* _pathRoot, tinygltf::Model& out_);

template <typename T>
inline frm::mat4 GetMatrix(const T* m)
{
	frm::mat4 ret;
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			ret[i][j] = (float)m[i * 4 + j];
		}
	}
	return ret;
}

// Shouldn't be required; the gltf exporter should correctly handle this.
inline frm::mat4& SwapMatrixYZ(const frm::mat4& m)
{
	frm::mat4 ret;
	ret.x = m.x;
	ret.y = m.z;
	ret.z = m.y;
	ret.w = m.w;
	return ret;
}

inline frm::mat4 GetTransform(const tinygltf::Node* node)
{
	if (node->matrix.empty())
	{
		frm::vec3 translation = frm::vec3(0.0f);
		if (!node->translation.empty())
		{
			FRM_ASSERT(node->translation.size() == 3);
			translation = frm::vec3(node->translation[0], node->translation[1], node->translation[2]);
		}

		frm::quat rotation = frm::quat(0.0f, 0.0f, 0.0f, 1.0f);
		if (!node->rotation.empty())
		{
			FRM_ASSERT(node->rotation.size() == 4);
			rotation = frm::quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
		}

		frm::vec3 scale = frm::vec3(1.0f);
		if (!node->scale.empty())
		{
			FRM_ASSERT(node->scale.size() == 3);
			scale = frm::vec3(node->scale[0], node->scale[1], node->scale[2]);
		}

		return frm::TransformationMatrix(translation, rotation, scale);
	}
	else
	{		
		FRM_ASSERT(node->matrix.size() == 16);
		return GetMatrix(node->matrix.data());
	}
}

inline frm::DataType GetDataType(int type)
{
	frm::DataType ret = frm::DataType_Invalid;

	switch (type)
	{
		default:                                     break;
		case TINYGLTF_COMPONENT_TYPE_BYTE:           ret = frm::DataType_Sint8;   break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  ret = frm::DataType_Uint8;   break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:          ret = frm::DataType_Sint16;  break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: ret = frm::DataType_Uint16;  break;
		case TINYGLTF_COMPONENT_TYPE_INT:            ret = frm::DataType_Sint32;  break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   ret = frm::DataType_Uint32;  break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:          ret = frm::DataType_Float32; break;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:         ret = frm::DataType_Float64; break;
	};

	FRM_ASSERT(ret != frm::DataType_Invalid);
	return ret;
}

inline size_t GetDataCount(int type)
{
	size_t ret = 0;

	switch (type)
	{
		default:                   break;
		case TINYGLTF_TYPE_SCALAR: ret = 1;  break;
		case TINYGLTF_TYPE_VEC2:   ret = 2;  break;
		case TINYGLTF_TYPE_VEC3:   ret = 3;  break;
		case TINYGLTF_TYPE_VEC4:   ret = 4;  break;
		case TINYGLTF_TYPE_MAT2:   ret = 4;  break;
		case TINYGLTF_TYPE_MAT3:   ret = 9;  break;
		case TINYGLTF_TYPE_MAT4:   ret = 16; break;
	};

	FRM_ASSERT(ret != 0);
	return ret;
}

struct AutoAccessor
{
	AutoAccessor(const Accessor& _accessor, const Model& _model)
		: m_accessor(_accessor)
		, m_bufferView(_model.bufferViews[m_accessor.bufferView])
	{
		m_count      = m_accessor.count;
		m_byteStride = m_accessor.ByteStride(m_bufferView);
		m_buffer     = _model.buffers[m_bufferView.buffer].data.data() + m_bufferView.byteOffset + m_accessor.byteOffset;
		m_bufferEnd  = m_buffer + m_count * m_byteStride;
	}

	bool next()
	{
		if (m_buffer >= m_bufferEnd)
		{
			return false;
		}

		m_buffer += m_byteStride;
		return true;
	}

	size_t getCount() const
	{
		return m_count;
	}

	template <typename T>
	T get() const
	{
		T ret;
		FRM_ASSERT(FRM_TRAITS_COUNT(T) == GetDataCount(m_accessor.type));
		frm::DataTypeConvert(GetDataType(m_accessor.componentType), FRM_DATA_TYPE_TO_ENUM(FRM_TRAITS_BASE_TYPE(T)), m_buffer, &ret, FRM_TRAITS_COUNT(T));
		return ret;
	}

	template <typename T>
	void copy(T* dst_) const
	{
		// \todo Optimize case where the src can be memcpy'd directly.
		FRM_ASSERT(FRM_TRAITS_COUNT(T) == GetDataCount(m_accessor.type));
		frm::DataTypeConvert(GetDataType(m_accessor.componentType), FRM_DATA_TYPE_TO_ENUM(FRM_TRAITS_BASE_TYPE(T)), m_buffer, dst_, FRM_TRAITS_COUNT(T) * m_count);
	}

	size_t getSizeBytes() const
	{
		return m_count * m_byteStride;
	}

	void copyBytes(void* dst_) const
	{
		memcpy(dst_, m_buffer, m_count * m_byteStride);
	}

private:

	Accessor             m_accessor;
	BufferView           m_bufferView;
	const unsigned char* m_buffer;
	const unsigned char* m_bufferEnd;
	size_t               m_byteStride;
	size_t               m_count;
};

} // namespace tinygltf
