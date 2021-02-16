#pragma once

#include <frm/core/frm.h>
#include <frm/core/FileSystem.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// FileSystemAsync
// Async file loading system.
//
// \todo
// - Batch API.
// - Internal design is very basic and relies on a single mutex with a single
//   producer thread issuing jobs.
////////////////////////////////////////////////////////////////////////////////
class FileSystemAsync
{
public:

	using JobID = void*;
	static constexpr JobID kInvalidJobID = JobID(0);

	// Call during application init. Set the number of loading threads.
	static bool  Init(int _threadCount = 1);

	// Call during application shutdown. Implicitly calls WaitAll().
	static void  Shutdown();

	// Return the status of a job. If true, the _jobID is implicitly released and set to kInvalidJobID.
	static bool  IsComplete(JobID& _jobID);

	// Wait until _jobID is complete. _jobID is implicitly released and set to kInvalidJobID. 
	static void  Wait(JobID& _jobID);
	
	// Wait until all pending jobs are complete.
	static void  WaitAll();

	// See FileSystem::Read.
	static JobID Read(File& file_, const char* _path = nullptr, int _root = FileSystem::GetDefaultRoot());

	// See FileSystem::ReadIfExists. Return kInvalidJobID if the file was not found.
	static JobID ReadIfExists(File& file_, const char* _path = nullptr, int _root = FileSystem::GetDefaultRoot());

	// See FileSystem::Write.
	static JobID Write(const File& _file, const char* _path = nullptr, int _root = FileSystem::GetDefaultRoot());

private:

	struct Impl;
	static Impl* s_impl;
};


} // namespace frm