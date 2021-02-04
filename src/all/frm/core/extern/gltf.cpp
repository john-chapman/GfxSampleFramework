#define TINYGLTF_IMPLEMENTATION
#include "gltf.h"

#include <frm/core/log.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/SkeletonAnimation.h>
#include <frm/core/Time.h>

#include <string>

using namespace frm;

static bool _FileExists(const std::string &abs_filename, void*)
{
	return FileSystem::Exists(abs_filename.c_str());
}

static std::string _ExpandFilePath(const std::string& path, void* userData)
{
	PathStr meshPath = ("%s/%s", (const char*)userData, path.c_str());
	return meshPath.c_str();
}

static bool _ReadWholeFile(std::vector<unsigned char>* out, std::string* err, const std::string& filepath, void*)
{
	File f;
	if (!FileSystem::ReadIfExists(f, filepath.c_str()))
	{
		return false;
	}

	const unsigned char* data = (unsigned char*)f.getData();
	const size_t dataSize = (size_t)f.getDataSize() - 1;
	out->assign(data, data + dataSize);
	err->assign("");

	return true;
}

static bool _WriteWholeFile(std::string* err, const std::string& filepath, const std::vector<unsigned char>& contents, void*)
{
	FRM_ASSERT(false);
	return false;
}

namespace tinygltf {

bool Load(const char* _srcData, size_t _srcDataSize, const char* _pathRoot, tinygltf::Model& out_)
{
	tinygltf::FsCallbacks callbacks;
	callbacks.FileExists     = &_FileExists;
	callbacks.ExpandFilePath = &_ExpandFilePath;
	callbacks.ReadWholeFile  = &_ReadWholeFile;
	callbacks.WriteWholeFile = &_WriteWholeFile;
	callbacks.user_data      = (void*)_pathRoot;

	tinygltf::TinyGLTF loader;
	loader.SetFsCallbacks(callbacks);

	std::string err, warn;
	if (!loader.LoadASCIIFromString(&out_, &err, &warn, _srcData, (unsigned int)_srcDataSize, ""))
	{
		FRM_LOG_ERR("Error: %s", err.c_str());
		return false;
	}
	
	if (!warn.empty())
	{
		FRM_LOG("Warning: %s", warn.c_str());
	}

	return true;
}

bool LoadSkeleton(const Model& _model, const Skin& _skin, eastl::vector<int>& _boneIndexMap_, frm::Skeleton& _skeleton_)
{
	if (_boneIndexMap_.size() != _model.nodes.size())
	{
		_boneIndexMap_.resize(_model.nodes.size(), -1);
	}

	// Create bones, get transform.
	for (int jointIndex : _skin.joints)
	{
		const Node& joint = _model.nodes[jointIndex];
		const int boneIndex = _skeleton_.addBone(joint.name.c_str(), -1);
		FRM_ASSERT(_boneIndexMap_[jointIndex] == -1);
		_boneIndexMap_[jointIndex] = boneIndex;

		Skeleton::Bone& bone = _skeleton_.getBone(boneIndex);
		if (!joint.translation.empty())
		{
			bone.m_translation = vec3(joint.translation[0], joint.translation[1], joint.translation[2]);
		}
		if (!joint.rotation.empty())
		{
			bone.m_rotation = quat(joint.rotation[0], joint.rotation[1], joint.rotation[2], joint.rotation[3]);
		}
		if (!joint.scale.empty())
		{
			bone.m_scale = vec3(joint.scale[0], joint.scale[1], joint.scale[2]);
		}
	}

	// Fixup parent indices.
	for (int jointIndex : _skin.joints)
	{
		const auto& joint = _model.nodes[jointIndex];
		const int parentIndex = _boneIndexMap_[jointIndex];
		for (int childIndex : joint.children)
		{
			int boneIndex = _boneIndexMap_[childIndex];
			FRM_ASSERT(parentIndex < boneIndex);
			_skeleton_.getBone(boneIndex).m_parentIndex = parentIndex;
		}
	}

	return true;
}

} // namespace gltf