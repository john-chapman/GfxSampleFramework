#include <frm/core/File.h>

#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/platform.h>
#include <frm/core/win.h>
#include <frm/core/FileSystem.h>
#include <frm/core/String.h>
#include <frm/core/TextParser.h>

#include <utility> // swap

namespace frm {

File::File()
{
	m_impl = INVALID_HANDLE_VALUE;
}

File::~File()
{
	if ((HANDLE)m_impl != INVALID_HANDLE_VALUE) 
	{
		FRM_PLATFORM_VERIFY(CloseHandle((HANDLE)m_impl));
	}
}

bool File::Exists(const char* _path)
{
	return GetFileAttributes(_path) != INVALID_FILE_ATTRIBUTES;
}

bool File::Read(File& file_, const char* _path)
{
	if (!_path) 
	{
		_path = file_.getPath();
	}
	FRM_ASSERT(_path);

	eastl::vector<char> data;
	bool  ret       = false;
	DWORD err       = 0;
	int   tryCount  = 5; // avoid sharing violations, especially when loading a file after a file change notification
	DWORD bytesRead = 0;

 	HANDLE h = INVALID_HANDLE_VALUE;
	do 
	{
		h = CreateFile(
			_path,
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
			);
		if (h == INVALID_HANDLE_VALUE) 
		{
			err = GetLastError();
			if (err == ERROR_SHARING_VIOLATION && tryCount > 0) 
			{
				FRM_LOG_DBG("Sharing violation reading '%s', retrying...", _path);
				Sleep(1);
				--tryCount;
			} 
			else
			{
				goto File_Read_end;
			}
		}
	} while (h == INVALID_HANDLE_VALUE);
	
	LARGE_INTEGER li;
	if (!GetFileSizeEx(h, &li)) 
	{
		err = GetLastError();
		goto File_Read_end;
	}
	data.resize((uint)li.QuadPart + 1);

	if (!ReadFile(h, data.data(), (DWORD)data.size() - 1, &bytesRead, 0)) // ReadFile can only read DWORD bytes
	{
		err = GetLastError();
		goto File_Read_end;
	}
	data.back() = '\0';

	ret = true;
	
  // close existing handle/free existing data
	if ((HANDLE)file_.m_impl != INVALID_HANDLE_VALUE) 
	{
		FRM_PLATFORM_VERIFY(CloseHandle((HANDLE)file_.m_impl));
	}
	
	swap(file_.m_data, data);
	file_.setPath(_path);

File_Read_end:
	if (!ret) 
	{
		FRM_LOG_ERR("Error reading '%s':\n\t%s", _path, GetPlatformErrorString((uint64)err));
	}
	if (h != INVALID_HANDLE_VALUE) 
	{
		FRM_PLATFORM_VERIFY(CloseHandle(h));
	}
	return ret;
}

bool File::Write(const File& _file, const char* _path)
{
	if (!_path) 
	{
		_path = _file.getPath();
	}
	FRM_ASSERT(_path);

	bool  ret  = false;
	DWORD err  = 0;
	
 	HANDLE h = CreateFile(
		_path,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);
	if (h == INVALID_HANDLE_VALUE) 
	{
		err = GetLastError();
		if (err == ERROR_PATH_NOT_FOUND) 
		{
			if (FileSystem::CreateDir(_path)) 
			{
				return Write(_file, _path);
			} 
			else 
			{
				return false;
			}
		} 
		else 
		{
			goto File_Write_end;
		}
	}

	DWORD dataSize = (DWORD)_file.getDataSize();
	DWORD bytesWritten;
	if (!WriteFile(h, _file.getData(), dataSize, &bytesWritten, NULL))
	{
		goto File_Write_end;
	}
	FRM_ASSERT(bytesWritten == dataSize);

	ret = true;

File_Write_end:
	if (!ret) 
	{
		FRM_LOG_ERR("Error writing '%s':\n\t%s", _path, GetPlatformErrorString((uint64)err));
	}
	if (h != INVALID_HANDLE_VALUE)
	{
		FRM_PLATFORM_VERIFY(CloseHandle(h));
	}
	return ret;
}

} // namespace frm