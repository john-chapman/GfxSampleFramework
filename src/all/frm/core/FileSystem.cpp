#include "FileSystem.h"

#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/File.h>
#include <frm/core/String.h>

#include <cstring>

using namespace frm;

FRM_DEFINE_STATIC_INIT(FileSystem, FileSystem::Init, FileSystem::Shutdown);

// PUBLIC

int FileSystem::AddRoot(const char* _path)
{
	auto it = eastl::find(s_roots->begin(), s_roots->end(), _path);
	if (it != s_roots->end())
	{
		return (int)(it - s_roots->begin());
	}
	
	int ret = (int)s_roots->size();
	s_roots->push_back(_path);
	Sanitize(s_roots->back());
	return ret;
}

bool FileSystem::Read(File& file_, const char* _path, int _root)
{
	PathStr fullPath;
	if (!FindExisting(fullPath, _path ? _path : file_.getPath(), _root))
	{
		FRM_LOG_ERR("Error loading '%s':\n\tFile not found", _path);
		//FRM_ASSERT(false);
		return false;
	}
	file_.setPath(fullPath.c_str());
	return File::Read(file_);
}

bool FileSystem::ReadIfExists(File& file_, const char* _path, int _root)
{
	PathStr fullPath;
	if (!FindExisting(fullPath, _path ? _path : file_.getPath(), _root))
	{
		return false;
	}
	file_.setPath(fullPath.c_str());
	return File::Read(file_);
}

bool FileSystem::Write(const File& _file, const char* _path, int _root)
{
	PathStr fullPath = MakePath(_path ? _path : _file.getPath(), _root);
	return File::Write(_file, (const char*)fullPath);
}

bool FileSystem::Exists(const char* _path, int _root)
{
	PathStr buf;
	return FindExisting(buf, _path, _root);
}

bool FileSystem::Matches(const char* _pattern, const char* _str)
{
// based on https://research.swtch.com/glob
	const size_t plen = strlen(_pattern);
	const size_t nlen = strlen(_str);
	size_t px = 0;
	size_t nx = 0;
	size_t nextPx = 0;
	size_t nextNx = 0;
	while (px < plen || nx < nlen)
	{
		if (px < plen)
		{
			char c = _pattern[px];
			switch (c)
			{
				case '?': // match 1 char
					if (nx < nlen)
					{
						px++;
						nx++;
						continue;
					}
					break;
					
				case '*': // match 0 or more char
				// try to match at nx, else restart at nx+1 next.
					nextPx = px;
					nextNx = nx + 1;
					px++;
					continue;
				
				default: // non-wildcard
					if (nx < nlen && _str[nx] == c)
					{
						px++;
						nx++;
						continue;
					}
					break;
			};
		}

		if (0 < nextNx && nextNx <= nlen)
		{
			px = nextPx;
			nx = nextNx;
			continue;
		}
		return false;
    }

    return true;
}

bool FileSystem::MatchesMulti(std::initializer_list<const char*> _patternList, const char* _str)
{
	for (auto& pattern : _patternList)
	{
		if (Matches(pattern, _str))
		{
			return true;
		}
	}
	return false;
}

PathStr FileSystem::MakePath(const char* _path, int _root)
{
	FRM_ASSERT(_root < (int)s_roots->size());
	const auto& root = (*s_roots)[_root];
	PathStr ret = _path;
	bool useRoot = !root.isEmpty() && !IsAbsolute(_path);
	if (useRoot)
	{
	 // check if the root already exists in path as a directory
		const char* r = strstr((const char*)root, _path);
		if (!r || *(r + root.getLength()) != '/')
		{
			ret.setf("%s%c%s", (const char*)root, '/', _path);
		}
	}
	Sanitize(ret);
	return ret;
}

PathStr FileSystem::StripPath(const char* _path)
{
	const char* beg = _path;
	while (*_path) {
		if (*_path == '/' || *_path == '\\')
		{
			beg = _path + 1;
		}
		++_path;
	}
	return PathStr(beg);
}

PathStr FileSystem::GetPath(const char* _path)
{
	const char* beg = _path;
	const char* end = _path;
	while (*_path) {
		if (*_path == '/' || *_path == '\\')
		{
			end = _path + 1;
		}
		++_path;
	}
	PathStr ret;
	if (end != beg) // empty path, in which case (end - beg) == 0 and StringBase::set will call strlen
	{
		ret.set(beg, end - beg);
	}
	return ret;
}

PathStr FileSystem::GetFileName(const char* _path)
{
	const char* beg = _path;
	while (*_path)
	{
		if (*_path == '/' || *_path == '\\')
		{
			beg = _path + 1;
		}
		++_path;
	}
	const char* end = beg;
	while (*end && *end != '.')
	{
		++end;
	}
	PathStr ret;
	ret.set(beg, end - beg);
	return ret;
}

const char* FileSystem::FindExtension(const char* _path)
{
	const char* ret = strrchr(_path, (int)'.');
	return ret == nullptr ? nullptr : ret + 1;
}

bool FileSystem::CompareExtension(const char* _ext, const char* _path)
{
	if (*_ext == '.')
	{
		++_ext;
	}
	const char* cmp = FindExtension(_path);
	if (cmp)
	{
		return _stricmp(_ext, cmp) == 0;
	}
	return false;
}

void FileSystem::SetExtension(PathStr& _path_, const char* _ext)
{
	if (*_ext == '.')
	{
		++_ext;
	}
	const char* ext = FindExtension(_path_.c_str());
	if (ext)
	{
		ext = --ext; // remove '.'
	}
	else
	{
		ext = _path_.end();
	}

	_path_.setLength(ext - _path_.begin());
	_path_.appendf(".%s", _ext);
}

const char* FileSystem::FindFileNameAndExtension(const char* _path)
{
	const char* ret = _path;
	while (*_path)
	{
		if (*_path == '/' || *_path == '\\')
		{
			ret = _path + 1;
		}
		++_path;
	}
	return ret;
}

// PRIVATE

int FileSystem::s_defaultRoot;
frm::storage<eastl::vector<PathStr> > FileSystem::s_roots;


void FileSystem::Init()
{
	*s_roots = eastl::vector<PathStr>();
}

void FileSystem::Shutdown()
{
}

bool FileSystem::FindExisting(PathStr& ret_, const char* _path, int _root)
{
	for (int i = _root; i > -1; --i)
	{
		ret_ = MakePath(_path, i);
		if (File::Exists((const char*)ret_))
		{
			return true;
		}
	}
	return false;
}
