#define TINYGLTF_IMPLEMENTATION
#include "gltf.h"

#include <frm/core/log.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
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

} // namespace gltf