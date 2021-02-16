#include "FileSystemAsync.h"

#include "frm/core/memory.h"
#include "frm/core/Log.h"
#include "frm/core/Pool.h"

#include <atomic>
#include <mutex>
#include <thread>
#define SCOPED_MUTEX_LOCK(mtx) std::lock_guard<std::mutex> FRM_UNIQUE_NAME(_scopedMutexLock)(mtx)

#include <EASTL/vector.h>

namespace frm {

struct FileSystemAsync::Impl
{
	enum class JobType : frm::uint8
	{
		Read,
		Write
	};
	
	struct Job
	{
		JobType           type;
		PathStr           path;
		int               root;
		File*             file;
		std::atomic<bool> complete;
	};

	eastl::vector<std::thread>  threads;
	bool                        threadLoopControl = true;
	
	eastl::vector<Job*>         activeJobs;
	eastl::vector<Job*>         jobQueue;
	Pool<Job>                   jobPool;
	std::mutex                  jobQueueMutex;
	
	Impl(int _threadCount);
	~Impl();

	Job* pushJob(JobType _type, File* _file_, const char* _path, int _root);
	Job* popJob();
	bool checkCompleteAndRelease(JobID _jobID);
	void flush();

	static void ThreadProc(FileSystemAsync::Impl* impl);
};

FileSystemAsync::Impl* FileSystemAsync::s_impl;
static std::thread::id s_mainThreadId; // Validate that certain functions are only called by the main thread.

// PUBLIC

bool FileSystemAsync::Init(int _threadCount)
{
	FRM_LOG("#FileSystemAsync::Init(%d)", _threadCount);

	s_impl = FRM_NEW(Impl(_threadCount));
	s_mainThreadId = std::this_thread::get_id();

	return true;
}

void FileSystemAsync::Shutdown()
{
	FRM_DELETE(s_impl);
}

bool FileSystemAsync::IsComplete(JobID& _jobID)
{
	bool ret = s_impl->checkCompleteAndRelease(_jobID);
	_jobID = ret ? kInvalidJobID : _jobID;
	return ret;
}

void FileSystemAsync::Wait(JobID& _jobID)
{
	while (!IsComplete(_jobID))
	{
		std::this_thread::yield();
	}
}

void FileSystemAsync::WaitAll()
{
	s_impl->flush();
}

FileSystemAsync::JobID FileSystemAsync::Read(File& file_, const char* _path, int _root)
{
	return s_impl->pushJob(Impl::JobType::Read, &file_, _path, _root);
}

FileSystemAsync::JobID FileSystemAsync::ReadIfExists(File& file_, const char* _path, int _root)
{
	if (FileSystem::Exists(_path ? _path : file_.getPath(), _root))
	{
		return Read(file_, _path, _root);
	}
	else
	{
		return kInvalidJobID;
	}
}

FileSystemAsync::JobID FileSystemAsync::Write(const File& _file, const char* _path, int _root)
{
	return s_impl->pushJob(Impl::JobType::Read, const_cast<File*>(&_file), _path, _root);
}


// PRIVATE

FileSystemAsync::Impl::Impl(int _threadCount)
	: jobPool(128)
{
	for (; _threadCount != -1; --_threadCount)
	{
		threads.push_back(std::thread(&ThreadProc, this));
	}
}

FileSystemAsync::Impl::~Impl()
{
	threadLoopControl = false;
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	threads.clear();

	// Assume we're quitting and just cancel all active jobs.
	jobQueue.clear();
	while (!activeJobs.empty())
	{
		jobPool.free(activeJobs.back());
		activeJobs.pop_back();
	}
}

FileSystemAsync::Impl::Job* FileSystemAsync::Impl::pushJob(JobType _type, File* _file_, const char* _path, int _root)
{
	FRM_ASSERT(std::this_thread::get_id() == s_mainThreadId);
	SCOPED_MUTEX_LOCK(jobQueueMutex);
	
	Job* job       = jobPool.alloc();
	job->type      = _type;
	job->path      = _path ? _path : _file_->getPath();
	job->root      = _root;
	job->file      = _file_;
	job->complete  .store(false);
	
	jobQueue.push_back(job);
	activeJobs.push_back(job);
	
	return job;
}

FileSystemAsync::Impl::Job* FileSystemAsync::Impl::popJob()
{
	SCOPED_MUTEX_LOCK(jobQueueMutex);

	Job* job = nullptr;
	if (!jobQueue.empty())
	{
		job = jobQueue.back();
		jobQueue.pop_back();
	}

	return job;
}

bool FileSystemAsync::Impl::checkCompleteAndRelease(JobID _jobID)
{
	FRM_ASSERT(std::this_thread::get_id() == s_mainThreadId);
	FRM_STRICT_ASSERT(_jobID != kInvalidJobID);

	//Job* job = findActiveJob(_jobID);
	FRM_ASSERT(sizeof(JobID) == sizeof(Job*));
	Job* job = (Job*)_jobID;
	
	if (job)
	{
		const bool complete = job->complete.load();
		if (complete)
		{
			SCOPED_MUTEX_LOCK(jobQueueMutex);
			auto it = eastl::find(activeJobs.begin(), activeJobs.end(), job);
			FRM_STRICT_ASSERT(it != activeJobs.end());
			activeJobs.erase_unsorted(it);
			jobPool.free(*it);
		}

		return complete;
	}
	else
	{
		// \todo In this case checkCompleteAndRelease() was called multiple times on the same job. This may be a valid use case if multiple
		// threads are waiting on the same job, in which case we need to handle JobID reuse more robustly.
		FRM_ASSERT(false);
		return true;
	}
}


void FileSystemAsync::Impl::flush()
{
	FRM_ASSERT(std::this_thread::get_id() == s_mainThreadId);

	while (!activeJobs.empty())
	{
		JobID jobID = (JobID)activeJobs.front();
		if (!checkCompleteAndRelease(jobID))
		{
			std::this_thread::yield();
		}
	}
}

void FileSystemAsync::Impl::ThreadProc(FileSystemAsync::Impl* impl)
{
	while (impl->threadLoopControl)
	{
		Job* job = impl->popJob();
		if (job)
		{
			switch (job->type)
			{
				default:
					FRM_ASSERT(false);
					break;
				case JobType::Read:
					FileSystem::Read(*job->file, job->path.c_str(), job->root);
					break;
				case JobType::Write:
					FileSystem::Write(*job->file, job->path.c_str(), job->root);
					break;
			};

			job->complete.store(true); // Calling Read() or Write() completes the job regardless of whether it succeeded.
		}
		else
		{
			std::this_thread::yield();
		}
	}
}

} // namespace frm
