#include <frm/core/FileSystem.h>

#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/platform.h>
#include <frm/core/win.h>
#include <frm/core/Pool.h>
#include <frm/core/String.h>
#include <frm/core/StringHash.h>
#include <frm/core/TextParser.h>

#include <Shlwapi.h>
#include <ShlObj.h>
#include <commdlg.h>
#include <cstring>

#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

#pragma comment(lib, "shlwapi")

using namespace frm;

static void BuildFilterString(std::initializer_list<const char*> _filterList, StringBase& ret_)
{
	for (size_t i = 0; i < _filterList.size(); ++i)
	{
		ret_.appendf("%s", *(_filterList.begin() + i));
		if (i != _filterList.size() - 1)
		{
			ret_.append(", ");
		}
	}
	ret_.append("\0", 1);

	for (auto& filter : _filterList)
	{
		ret_.appendf("%s;", filter);
	}
	ret_.append("\0", 1);
}

static DateTime FileTimeToDateTime(const FILETIME& _fileTime)
{
	LARGE_INTEGER li;
	li.LowPart  = _fileTime.dwLowDateTime;
	li.HighPart = _fileTime.dwHighDateTime;
	return DateTime(li.QuadPart);
}

static bool GetFileDateTime(const char* _fullPath, DateTime& created_, DateTime& modified_)
{
#if 0
	const char* err = nullptr;
	HANDLE h = CreateFile(
		_fullPath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
		);
	if (h == INVALID_HANDLE_VALUE)
	{
		err = GetPlatformErrorString(GetLastError());
		goto GetFileDateTime_End;
	}
	FILETIME created, modified;
	if (GetFileTime(h, &created, NULL, &modified) == 0)
	{
		err = GetPlatformErrorString(GetLastError());
		goto GetFileDateTime_End;
	}	
	created_ = FileTimeToDateTime(created);
	modified_ = FileTimeToDateTime(modified);
	
GetFileDateTime_End:
	if (err)
	{
		FRM_LOG_ERR("GetFileDateTime: %s", err);
		FRM_ASSERT(false);
	}
	FRM_PLATFORM_VERIFY(CloseHandle(h));
	return err == nullptr;
#else
	const char* err = nullptr;
	WIN32_FILE_ATTRIBUTE_DATA attr;
	if (GetFileAttributesEx(_fullPath, GetFileExInfoStandard, &attr) == 0)
	{
		err = (const char*)GetPlatformErrorString(GetLastError());
		goto GetFileDateTime_End;
	}
	created_ = FileTimeToDateTime(attr.ftCreationTime);
	modified_ = FileTimeToDateTime(attr.ftLastWriteTime);
GetFileDateTime_End:
	if (err)
	{
		FRM_LOG_ERR("GetFileDateTime: %s", err);
		FRM_ASSERT(false);
	}
	return err == nullptr;
#endif
}

static void GetAppPath(TCHAR ret_[MAX_PATH], const char* _append = nullptr)
{
	TCHAR tmp[MAX_PATH];

	FRM_PLATFORM_VERIFY(GetModuleFileName(0, tmp, MAX_PATH));
	FRM_PLATFORM_VERIFY(GetFullPathName(tmp, MAX_PATH, ret_, NULL)); // GetModuleFileName can return a relative path (e.g. when launching from the ide)

	if (_append && *_append != '\0')
	{
		FRM_PLATFORM_VERIFY(GetFullPathName(_append, MAX_PATH, tmp, NULL));
		char* pathEnd = strrchr(ret_, (int)'\\');
		++pathEnd;
		strcpy(pathEnd, _append);
	}
	else
	{
		char* pathEnd = strrchr(ret_, (int)'\\');
		++pathEnd;
		*pathEnd = '\0';
	}
}

// PUBLIC

bool FileSystem::Delete(const char* _path)
{
	if (DeleteFile(_path) == 0)
	{
		DWORD err = GetLastError();
		if (err != ERROR_FILE_NOT_FOUND)
		{
			FRM_LOG_ERR("DeleteFile(%s): %s", _path, GetPlatformErrorString(err));
		}
		return false;
	}
	return true;
}

DateTime FileSystem::GetTimeCreated(const char* _path, int _rootHint)
{
	PathStr fullPath;
	if (!FindExisting(fullPath, _path, _rootHint))
	{
		return DateTime(); // \todo return invalid sentinel
	}
	DateTime created, modified;
	GetFileDateTime((const char*)fullPath, created, modified);
	return created;
}

DateTime FileSystem::GetTimeModified(const char* _path, int _rootHint)
{
	PathStr fullPath;
	if (!FindExisting(fullPath, _path, _rootHint))
	{
		return DateTime(); // \todo return invalid sentinel
	}
	DateTime created, modified;
	GetFileDateTime((const char*)fullPath, created, modified);
	return modified;
}

bool FileSystem::CreateDir(const char* _path)
{
	TextParser tp(_path);
	while (tp.advanceToNext("\\/") != 0)
	{
		String<64> mkdir;
		mkdir.set(_path, tp.getCharCount());
		if (CreateDirectory((const char*)mkdir, NULL) == 0)
		{
			DWORD err = GetLastError();
			if (err != ERROR_ALREADY_EXISTS)
			{
				FRM_LOG_ERR("CreateDirectory(%s): %s", _path, GetPlatformErrorString(err));
				return false;
			}
		}
		tp.advance(); // skip the delimiter
	}
	return true;
}

PathStr FileSystem::MakeRelative(const char* _path, int _root)
{
 // \todo fix this function! it needs to work correctly in the following cases:
 // 1. _path contains *any* of the application roots = strip the path up to and including the root dir.
 // 2. _path contains *no* application roots but has a common prefix with the app path = make a relative path with ../
 // 3. As .2 but with no common prefix, path is absolute so do nothing.

	TCHAR root[MAX_PATH] = {};
	if (IsAbsolute((const char*)(*s_roots)[_root]))
	{
		FRM_PLATFORM_VERIFY(GetFullPathName((const char*)(*s_roots)[_root], MAX_PATH, root, NULL));
	}
	else
	{
		GetAppPath(root, (const char*)(*s_roots)[_root]);
	}

 // construct the full path
	TCHAR path[MAX_PATH] = {};
	FRM_PLATFORM_VERIFY(GetFullPathName(_path, MAX_PATH, path, NULL));

	char tmpbuf[MAX_PATH];
	char* tmp = tmpbuf;
 // PathRelativePathTo will fail if tmp and root don't share a common prefix
	FRM_VERIFY(PathRelativePathTo(tmp, root, FILE_ATTRIBUTE_DIRECTORY, path, PathIsDirectory(path) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL));
	while (*tmp == '.' || *tmp == '\\' || *tmp == '/')
	{
		++tmp;
	}
	PathStr ret(tmp);
	ret.replace('\\', '/');
	return ret;
}

bool FileSystem::IsAbsolute(const char* _path)
{
	return PathIsRelative(_path) == FALSE;
}

PathStr FileSystem::StripRoot(const char* _path)
{
	TCHAR path[MAX_PATH] = {};
	FRM_PLATFORM_VERIFY(GetFullPathName(_path, MAX_PATH, path, NULL));

	for (auto& root : (*s_roots))
	{
		if (root.isEmpty())
		{
			continue;
		}
		TCHAR pathRoot[MAX_PATH];
		if (IsAbsolute((const char*)root))
		{
			FRM_PLATFORM_VERIFY(GetFullPathName((const char*)root, MAX_PATH, pathRoot, NULL));
		}
		else
		{
			GetAppPath(pathRoot, (const char*)root);
		}
		const char* rootBeg = strstr(path, pathRoot);
		if (rootBeg != nullptr)
		{
			PathStr ret(path + strlen(pathRoot) + 1);
			ret.replace('\\', '/');
			return ret;
		}
	}
 // no root found, strip the whole path if not absolute
	if (!IsAbsolute(_path))
	{
		return StripPath(_path);
	}
	return _path;
}

bool FileSystem::PlatformSelect(PathStr& ret_, std::initializer_list<const char*> _filterList)
{
	static DWORD       s_filterIndex = 0;
	static const DWORD kMaxOutputLength = MAX_PATH;
	static char        s_output[kMaxOutputLength];

	PathStr filters;
	BuildFilterString(_filterList, filters);

	OPENFILENAMEA ofn   = {};
	ofn.lStructSize     = sizeof(ofn);
	ofn.lpstrFilter     = (LPSTR)filters;
	ofn.nFilterIndex    = s_filterIndex;
	ofn.lpstrInitialDir = (LPSTR)(*s_roots)[s_defaultRoot];
	ofn.lpstrFile       = s_output;
	ofn.nMaxFile        = kMaxOutputLength;
	ofn.lpstrTitle      = "File";
	ofn.Flags           = OFN_DONTADDTORECENT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	if (GetOpenFileName(&ofn) != 0)
	{
		s_filterIndex = ofn.nFilterIndex;
	 // parse s_output into results_
		ret_.set(s_output);
		Sanitize(ret_);
		return true;
	}
	else
	{
		DWORD err = CommDlgExtendedError();
		if (err != 0)
		{
			FRM_LOG_ERR("GetOpenFileName (0x%x)", err);
			FRM_ASSERT(false);
		}		
	}
	return false;
}

bool FileSystem::PlatformSelectDir(PathStr& ret_, const char* _prompt)
{
	BROWSEINFO bi = { 0 };
	bi.lpszTitle  = _prompt;
	bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lParam     = (LPARAM)ret_.c_str();
	
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl != 0 )
	{
        TCHAR path[MAX_PATH];
        SHGetPathFromIDList(pidl, path);
		ret_ = Sanitize(path);		

        IMalloc* imalloc = nullptr;
        if (SUCCEEDED(SHGetMalloc(&imalloc)))
		{
            imalloc->Free(pidl);
            imalloc->Release();
        }

        return true;
    }

	return false;
}

int FileSystem::PlatformSelectMulti(PathStr retList_[], int _maxResults, std::initializer_list<const char*> _filterList)
{
	static DWORD       s_filterIndex = 0;
	static const DWORD kMaxOutputLength = 1024 * 4;
	static char        s_output[kMaxOutputLength];

	PathStr filters;
	BuildFilterString(_filterList, filters);

	OPENFILENAMEA ofn   = {};
	ofn.lStructSize     = sizeof(ofn);
	ofn.lpstrFilter     = (LPSTR)filters;
	ofn.nFilterIndex    = s_filterIndex;
	ofn.lpstrInitialDir = (LPSTR)(*s_roots)[s_defaultRoot];
	ofn.lpstrFile       = s_output;
	ofn.nMaxFile        = kMaxOutputLength;	
	ofn.lpstrTitle      = "File";
	ofn.Flags           = OFN_ALLOWMULTISELECT | OFN_DONTADDTORECENT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	if (GetOpenFileName(&ofn) != 0)
	{
		s_filterIndex = ofn.nFilterIndex;

	 // parse s_output into results_
		int ret = 0;
		TextParser tp(s_output);
		while (ret < _maxResults)
		{
			tp.advanceToNext('\0');
			tp.advance();
			if (*tp == '\0')
			{
				break;
			}
			retList_[ret].appendf("%s\\%s", s_output, (const char*)tp);
			Sanitize(retList_[ret]);
			++ret;
		}
		return ret;

	}
	else
	{
		DWORD err = CommDlgExtendedError();
		if (err != 0)
		{
			FRM_LOG_ERR("GetOpenFileName (0x%x)", err);
			FRM_ASSERT(false);
		}		
	}
	return 0;
}

int FileSystem::ListFiles(PathStr retList_[], int _maxResults, const char* _path, std::initializer_list<const char*> _filterList, bool _recursive)
{
	eastl::vector<PathStr> dirs;
	dirs.push_back(_path);
	int ret = 0;
	while (!dirs.empty())
	{
		PathStr root = (PathStr&&)dirs.back();
		dirs.pop_back();
		root.replace('/', '\\'); // opposite of Sanitize()
		PathStr search = root;
		search.appendf("\\*"); // ignore filter here, need to catch dirs for recursion

		WIN32_FIND_DATA ffd;
		HANDLE h = FindFirstFile((const char*)search, &ffd);
		if (h == INVALID_HANDLE_VALUE)
		{
			DWORD err = GetLastError();
			if (err != ERROR_FILE_NOT_FOUND)
			{
				FRM_LOG_ERR("ListFiles (FindFirstFile): %s", GetPlatformErrorString(err));
			}
			continue;
		} 

		do
		{
			if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
			{
				if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (_recursive)
					{
						dirs.push_back(root);
						dirs.back().appendf("\\%s", ffd.cFileName);
					}
				}
				else
				{
					if (MatchesMulti(_filterList, (const char*)ffd.cFileName))
					{
						if (ret < _maxResults)
						{
							retList_[ret].setf("%s\\%s", (const char*)root, ffd.cFileName);
							Sanitize(retList_[ret]);
						}
						++ret;
					}
				}
			}
	
        } while (FindNextFile(h, &ffd) != 0);

		DWORD err = GetLastError();
		if (err != ERROR_NO_MORE_FILES)
		{
			FRM_LOG_ERR("ListFiles (FindNextFile): %s", GetPlatformErrorString(err));
		}

		FindClose(h);
    }

	return ret;
}

int FileSystem::ListDirs(PathStr retList_[], int _maxResults, const char* _path, std::initializer_list<const char*> _filterList, bool _recursive)
{
	eastl::vector<PathStr> dirs;
	dirs.push_back(_path);
	int ret = 0;
	// There are 2 choices of behavior here: 'direct' recursion (where sub directories appear immediately after the parent in the list), or 'deferred' 
	// recursion, which is what is implemented. In theory, the latter is better because you can fill a small list of the first couple of levels of 
	// the hierarchy and then manually recurse into those directories as needed.
	while (!dirs.empty())
	{
		PathStr root = (PathStr&&)dirs.back();
		dirs.pop_back();
		root.replace('/', '\\');
		PathStr search = root;
		search.appendf("\\*");
	
		WIN32_FIND_DATA ffd;
		HANDLE h = FindFirstFile((const char*)search, &ffd);
		if (h == INVALID_HANDLE_VALUE)
		{
			DWORD err = GetLastError();
			if (err != ERROR_FILE_NOT_FOUND)	
			{
				FRM_LOG_ERR("ListFiles (FindFirstFile): %s", GetPlatformErrorString(err));
			}
			continue;
		}

		do
		{
			if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
			{
				if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (_recursive)
					{
						dirs.push_back(root);
						dirs.back().appendf("\\%s", ffd.cFileName);
					}
					if (MatchesMulti(_filterList, (const char*)ffd.cFileName))
					{
						if (ret < _maxResults)
						{
							retList_[ret].setf("%s\\%s", (const char*)root, ffd.cFileName);
							Sanitize(retList_[ret]);
						}
						++ret;
					}
				}
			}
        } while (FindNextFile(h, &ffd) != 0);

		DWORD err = GetLastError();
		if (err != ERROR_NO_MORE_FILES)
		{
			FRM_LOG_ERR("ListFiles (FindNextFile): %s", GetPlatformErrorString(err));
		}

		FindClose(h);
    }

	return ret;
}


namespace {
/* Notes:
	- Changes within symbolic link subdirs don't generate events.
	- Deleting or moving a dir doesn't generate events for its subtree. Copying does.
	- Duplicate 'modified' actions are received consecutively* but may be split over 2 calls to DispatchNotifications(),
	  hence store the last received action inside the watch struct (the queue gets cleared, so can't use queue.back()).
		* This isn't certain!
	- Other processes may modify files in arbitrary, unpredictable ways e.g. writing to a new file, deleting the old one
	  and renaming the new one. You may therefore get a FileAction_Created event where you were expecting a FileAction_Modified.
	  See the comments on this page: https://qualapps.blogspot.com/2010/05/understanding-readdirectorychangesw_19.html
		- \todo potentially fix this by detecting if a file is deleted and then immediately created again?
*/
	struct Watch
	{
		OVERLAPPED m_overlapped = {};
		HANDLE     m_hDir       = INVALID_HANDLE_VALUE;
		DWORD      m_filter     = 0;
		UINT       m_bufSize    = 1024 * 32; // 32kb
		BYTE*      m_buf        = NULL;
		PathStr    m_dirPath    = "";

		eastl::pair<PathStr, FileSystem::FileAction> m_prevAction;
		FileSystem::FileActionCallback* m_dispatchCallback;
		eastl::vector<eastl::pair<PathStr, FileSystem::FileAction> > m_dispatchQueue;
	};
	static Pool<Watch> s_WatchPool(8);
	static eastl::vector_map<StringHash, Watch*> s_WatchMap;

	void CALLBACK WatchCompletion(DWORD _err, DWORD _bytes, LPOVERLAPPED _overlapped);
	void          WatchUpdate(Watch* _watch);


	void CALLBACK WatchCompletion(DWORD _err, DWORD _bytes, LPOVERLAPPED _overlapped)
	{
		if (_err == ERROR_OPERATION_ABORTED) // CancellIo was called
		{
			return;
		}
		if (_err != ERROR_SUCCESS)
		{
			FRM_LOG_ERR("FileSystem: completion routine error '%s'", GetPlatformErrorString(_err));
			return;
		}
		if (_bytes == 0) // overflow? notifications lost in this case?
		{
			FRM_LOG("FileSystem: completion routine called with 0 bytes");
			return;
		}
//FRM_LOG_DBG("---");
		Watch* watch = (Watch*)_overlapped; // m_overlapped is the first member, so this works

		TCHAR fileName[MAX_PATH];
		for (DWORD off = 0;;)
		{
			PFILE_NOTIFY_INFORMATION info = (PFILE_NOTIFY_INFORMATION)(watch->m_buf + off);		
			off += info->NextEntryOffset;

		 // unicode -> utf8
			int count = WideCharToMultiByte(CP_UTF8, 0, info->FileName, info->FileNameLength / sizeof(WCHAR), fileName, MAX_PATH - 1, NULL, NULL);
			fileName[count] = '\0';

			FileSystem::FileAction action = FileSystem::FileAction_Count;
//char* actionStr = "UNKNOWN";
			switch (info->Action)
			{
				case FILE_ACTION_ADDED: 
				case FILE_ACTION_RENAMED_NEW_NAME:
					action = FileSystem::FileAction_Created;
//actionStr = "FILE_ACTION_ADDED";
					break;
				case FILE_ACTION_REMOVED:
				case FILE_ACTION_RENAMED_OLD_NAME:
					action = FileSystem::FileAction_Deleted; 
//actionStr = "FILE_ACTION_REMOVED";
					break;
				case FILE_ACTION_MODIFIED:
				default:
					action = FileSystem::FileAction_Modified;
//actionStr = "FILE_ACTION_MODIFIED";
					break;
			};

		 // check to see if the action was duplicated - this happens often for FILE_ACTION_MODIFIED
			bool duplicate = false;
			std::replace(fileName, fileName + count, '\\', '/');
			auto& prev = watch->m_prevAction;
			if (prev.second == action) // action matches
			{
				if (prev.first.getLength() == count) // path length matches
				{
					if (memcmp(prev.first.c_str(), fileName, count) == 0) // compare the strings
					{
						duplicate = true;
					}
				}
			}
//FRM_LOG_DBG("%s -- %s%s", internal::StripPath(fileName), actionStr, duplicate ? " (DUPLICATE)" : "");
			if (!duplicate)
			{
				watch->m_prevAction = eastl::make_pair(PathStr(fileName), action);
				watch->m_dispatchQueue.push_back(watch->m_prevAction);
			}
			
			if (info->NextEntryOffset == 0)
			{
				break;
			}
		}

	 // reissue ReadDirectoryChangesW; it seems that we don't actually miss any notifications which happen between the start of the completion routine
	 // and the reissue, so it's safe to wait until the dispatch is done
		WatchUpdate(watch);
	}

	void WatchUpdate(Watch* _watch)
	{
		FRM_PLATFORM_VERIFY(ReadDirectoryChangesW(
			_watch->m_hDir,
			_watch->m_buf,
			_watch->m_bufSize,
			TRUE, // watch subtree
			_watch->m_filter,
			NULL,
 			&_watch->m_overlapped, 
			WatchCompletion
			));
	}
}

void FileSystem::BeginNotifications(const char* _dir, FileActionCallback* _callback)
{
	StringHash dirHash(_dir);
	if (s_WatchMap.find(dirHash) != s_WatchMap.end())
	{
		FRM_ASSERT(false);
		return;
	}
	CreateDirectoryA(_dir, NULL); // create if it doesn't already exist

	Watch* watch = s_WatchPool.alloc();
	watch->m_dirPath = _dir;
	watch->m_hDir = CreateFileA(
		_dir,                                                    // path
		FILE_LIST_DIRECTORY,                                     // desired access
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,  // share mode
		NULL,                                                    // security attribs
		OPEN_EXISTING,                                           // create mode
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,       // file attribs
		NULL                                                     // template handle
		);
	FRM_PLATFORM_ASSERT(watch->m_hDir != INVALID_HANDLE_VALUE);

	s_WatchMap[dirHash] = watch;
	watch->m_buf = (BYTE*)FRM_MALLOC_ALIGNED(watch->m_bufSize, sizeof(DWORD));
	watch->m_filter = 0
			| FILE_NOTIFY_CHANGE_CREATION
			| FILE_NOTIFY_CHANGE_SIZE
			| FILE_NOTIFY_CHANGE_ATTRIBUTES 
			| FILE_NOTIFY_CHANGE_FILE_NAME 
			| FILE_NOTIFY_CHANGE_DIR_NAME 
			;
	watch->m_dispatchCallback = _callback;
	WatchUpdate(watch);
}

void FileSystem::EndNotifications(const char* _dir)
{
	StringHash dirHash(_dir);
	auto it = s_WatchMap.find(dirHash);
	if (it == s_WatchMap.end())
	{
		FRM_ASSERT(false);
		return;
	}
	auto watch = it->second;
	FRM_PLATFORM_VERIFY(CancelIo(watch->m_hDir));
	SleepEx(0, TRUE); // flush any pending calls to the completion routine
	FRM_PLATFORM_VERIFY(CloseHandle(watch->m_hDir));
	FRM_FREE_ALIGNED(watch->m_buf);
	s_WatchPool.free(watch);
	s_WatchMap.erase(it);
}

void FileSystem::DispatchNotifications(const char* _dir)
{
 // clear 'prevAction' - identical consecutive actions *between* calls to DispatchNotifications are allowed
	if (_dir)
	{
		auto it = s_WatchMap.find(StringHash(_dir));
		if (it == s_WatchMap.end())
		{
			FRM_ASSERT(false);
			return;
		}
		it->second->m_prevAction.second = FileAction_Count;

	}
	else
	{
		for (auto& it : s_WatchMap)
		{
			it.second->m_prevAction.second = FileAction_Count;
		}
	}

 // let the kernel call the completion routine and fill the dispatch queues
	SleepEx(0, TRUE);

 // dispatch
	if (_dir)
	{
		auto it = s_WatchMap.find(StringHash(_dir));
		Watch& watch = *it->second;
		for (auto& file : watch.m_dispatchQueue)
		{
			PathStr filePath("%s/%s", watch.m_dirPath.c_str(), file.first.c_str());
			watch.m_dispatchCallback(filePath.c_str(), file.second);
		}
		watch.m_dispatchQueue.clear();

	}
	else
	{
		for (auto& it : s_WatchMap)
		{
			Watch& watch = *it.second;
			for (auto& file : watch.m_dispatchQueue)
			{
				PathStr filePath("%s/%s", watch.m_dirPath.c_str(), file.first.c_str());
				watch.m_dispatchCallback(filePath.c_str(), file.second);
			}
			watch.m_dispatchQueue.clear();
		}
	}
}
