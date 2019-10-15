#pragma once

#include <frm/core/frm.h>
#include <frm/core/memory.h>
#include <frm/core/File.h>
#include <frm/core/String.h>
#include <frm/core/StaticInitializer.h>
#include <frm/core/Time.h>

#include <EASTL/vector.h>
#include <initializer_list>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// FileSystem
// File system operations, file access management, path manipulation helpers.
//
// Multiple search paths ('roots') may be set. These are searched in reverse 
// order when reading a file. The order is determined by the sequence of calls
// to AddRoot().
//
// \todo https://github.com/john-chapman/ApplicationTools/issues/21
////////////////////////////////////////////////////////////////////////////////
class FileSystem
{
public:

	// Add a root path. Return index in the root table.
	static int         AddRoot(const char* _path);

	// _root was previously returned by AddRoot().
	static const char* GetRoot(int _root)          { return (const char*)(*s_roots)[_root]; }

	// Get/set the default root path. 
	static void        SetDefaultRoot(int _root)   { FRM_ASSERT(_root < (int)s_roots->size()); s_defaultRoot = _root; }
	static int         GetDefaultRoot()            { return s_defaultRoot; }

 // File operations

	// Read a file into memory. Each root is searched in reverse order, beginning at _root. If _path is 0, file_.getPath() is used.
	// Return false if an error occurred, in which case file_ remains unchanged. On success, any resources already associated 
	// with file_ are released. _root is ignored if _path is absolute.
	static bool        Read(File& file_, const char* _path = nullptr, int _root = GetDefaultRoot());

	// As Read() but first checks if the file exists. Return false if the file does not exist or if an error occurred.
	static bool        ReadIfExists(File& file_, const char* _path = nullptr, int _root = GetDefaultRoot());

	// Write _file's data to _path. If _path is 0, _file.getPath() is used. Return false if an error occurred, in which case 
	// any existing file at _path may or may not have been overwritten. _root is ignored if _path is absolute.
	static bool        Write(const File& _file, const char* _path = nullptr, int _root = GetDefaultRoot());

	// Return true if _path exists. Each root is searched, beginning at _root.
	static bool        Exists(const char* _path, int _root = GetDefaultRoot());

	// Delete a file.
	static bool        Delete(const char* _path);

	// Get the creation/last modified time for a file. _path is treated as per Read(). 
	static DateTime    GetTimeCreated(const char* _path, int _root = GetDefaultRoot());
	static DateTime    GetTimeModified(const char* _path, int _root = GetDefaultRoot());

	// Create the directory specified by _path, plus all parent directories if they do not exist. Return false if an error occurred.
	// If _path contains only directory names, it must end in a path separator (e.g. "dir0/dir1/").
	static bool        CreateDir(const char* _path);

 // Path manipulation

	// s_roots[_root] + kPathSeparator + _path. _root is ignored if _path is absolute.
	static PathStr     MakePath(const char* _path, int _root = GetDefaultRoot());

	// Match _str against _pattern with wildcard characters: '?' matches a single character, '*' matches zero or more characters.
	static bool        Matches(const char* _pattern, const char* _str);
	// Call Matches() for each of a list of patterns e.g. { "*.txt", "*.png" }.	
	static bool        MatchesMulti(std::initializer_list<const char*> _patternList, const char* _str);

	// Make _path relative to _root.
	static PathStr     MakeRelative(const char* _path, int _root = GetDefaultRoot());
	// Return true if _path is absolute.
	static bool        IsAbsolute(const char* _path);

	// Strip any root from _path, or the whole path if _path is absolute.
	static PathStr     StripRoot(const char* _path);
	// Strip path from _path.
	static PathStr     StripPath(const char* _path);

	// Replace '\' with '/'.
	static void        Sanitize(PathStr& _path_)   { _path_.replace('\\', '/'); }
	static PathStr     Sanitize(const char* _path) { PathStr ret = _path; ret.replace('\\', '/'); return ret; }

	// Extract path from _path (remove file name + extension).
	static PathStr     GetPath(const char* _path);
	// Extract file name from _path (remove path + extension).
	static PathStr     GetFileName(const char* _path);
	// Extract extension from _path (remove path + file name).
	static PathStr     GetExtension(const char* _path) { return PathStr(FindExtension(_path)); }

	// Return ptr to the character following the last occurrence of '.' in _path.
	static const char* FindExtension(const char* _path);
	// Return ptr to the character following the last occurrence of '\' or '/' in _path.
	static const char* FindFileNameAndExtension(const char* _path);
	// Compare _ext with the extension from _path (case insensitive).
	static bool        CompareExtension(const char* _ext, const char* _path);
	// Set the extension.
	static void        SetExtension(PathStr& _path_, const char* _ext);
	
	// Select a file/files via the platform UI.
	static bool        PlatformSelect(PathStr& ret_, std::initializer_list<const char*> _filterList = { "*" });
	static int         PlatformSelectMulti(PathStr retList_[], int _maxResults, std::initializer_list<const char*> _filterList = { "*" });
	// Select a dir via the platform UI.
	static bool        PlatformSelectDir(PathStr& ret_, const char* _prompt = "");

 // Inspection

	// List up to _maxResults files in _path, with optional recursion. Return the number of files which would be found if not limited by _maxResults.
	static int         ListFiles(PathStr retList_[], int _maxResults, const char* _path, std::initializer_list<const char*> _filterList = { "*" }, bool _recursive = false);
	// List up to _maxResults dirs in _path, with optional recursion. _filters is a null-separated list of filter strings. Return the number of dirs which would be found if not limited by _maxResults.
	static int         ListDirs(PathStr retList_[], int _maxResults, const char* _path, std::initializer_list<const char*> _filterList= { "*" }, bool _recursive = false);


 // File action notifications

	enum FileAction_
	{
		FileAction_Created,
		FileAction_Deleted,
		FileAction_Modified,

		FileAction_Count
	};
	typedef int FileAction;

	typedef void (FileActionCallback)(const char* _path, FileAction _action);

	// Begin receiving notifications for changes to _dir (and its subtree). _callback will be called once for each event. See DispatchNotifcations().
	static void        BeginNotifications(const char* _dir, FileActionCallback* _callback);
	// Stop receiving notifications for changes to _dir.
	static void        EndNotifications(const char* _dir);
	// Dispatch file action notifications. If _dir is 0, dispatch to all registered callbacks. This should be called frequently.
	static void        DispatchNotifications(const char* _dir = nullptr);

private:

	static int s_defaultRoot;
	static storage<eastl::vector<PathStr> > s_roots;
	
	FRM_DECLARE_STATIC_INIT_FRIEND(FileSystem);
	static void Init();
	static void Shutdown();

	// Get a path to an existing file based on _path and _root. Return false if no existing file was found.
	static bool FindExisting(PathStr& ret_, const char* _path, int _root);

};
FRM_DECLARE_STATIC_INIT(FileSystem);

} // namespace frm
