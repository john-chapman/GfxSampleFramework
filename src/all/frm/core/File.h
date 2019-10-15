#pragma once

#include <frm/core/frm.h>
#include <frm/core/String.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// File
// An implicit null character is appended to the file data on read, so it is 
// safe to interpret the file as a C string.
// \todo API should include some interface for either writing to the internal 
//   buffer directly, or setting the buffer ptr without copying all the data
//   (prefer the former, buffer ownership issues in the latter case).
// \todo Checksum/hash util.
////////////////////////////////////////////////////////////////////////////////
class File: private non_copyable<File>
{
public:

	File();
	~File();

	// Return true if _path exists.
	static bool Exists(const char* _path);

	// Read file into memory from _path (or file_.getPath() by default). Use getData() to access the resulting buffer. Return false if 
	// an error occurred. On success, any resources previously associated with file_ are released.
	static bool Read(File& file_, const char* _path = nullptr);

	// Write file to _path (or _file.getPath() by default). Return false if an error occurred, in which case an existing file at _path 
	// may or may not have been overwritten.
	static bool Write(const File& _file, const char* _path = nullptr);

	// Allocate _size bytes for the internal buffer and optionally copy from _data. If _data is nullptr, the internal buffer is resized
	// and filled with zeroes.
	void        setData(const char* _data, uint _size);

	// Append _size bytes from _data to the internal buffer. If _data is nullptr, the internal buffer is resized and zeroes are appended.
	void        appendData(const char* _data, uint _size);

	// Resize the internal data buffer to _capacity.
	void        reserveData(uint _capacity);

	const char* getPath() const              { return (const char*)m_path; }
	void        setPath(const char* _path)   { m_path.set(_path); }
	const char* getData() const              { return m_data.data(); }
	char*       getData()                    { return m_data.data(); }
	uint        getDataSize() const          { return m_data.size(); }
	uint        getDataCapacity() const      { return m_data.capacity(); }

private:

	PathStr             m_path  = "";
	void*               m_impl  = nullptr;
	eastl::vector<char> m_data;
};

} // namespace frm
