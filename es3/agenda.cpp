#include "agenda.h"

#include <unistd.h>
#include <boost/threadpool.hpp>
#include "uploader.h"

using namespace es3;

//Utility thread pool
boost::threadpool::pool the_pool(
//#ifdef NDEBUG
	sysconf(_SC_NPROCESSORS_ONLN)+8
//#else
//	1
//#endif
);

agenda_ptr agenda::make_new()
{
	return agenda_ptr(new agenda());
}

agenda::agenda()
{

}

struct task_executor
{
	sync_task_ptr task_;
	agenda_ptr agenda_;
	void operator ()()
	{
		for(int f=0; f<10; ++f)
		{
			try
			{
				(*task_)(agenda_);
				return;
			} catch (const es3_exception &ex)
			{
				const result_code_t &code = ex.err();
				if (code.code()==errNone)
				{
					VLOG(1) << ex.what();
					continue;
				} else if (code.code()==errWarn)
				{
					VLOG(0) << ex.what();
					continue;
				} else
				{
					VLOG(0) << ex.what();
					break;
				}
			}
		}
	}
};

void agenda::schedule_upload(const connection_data &data,
							 const boost::filesystem::path &path,
							 const std::string &remote,
							 const std::string &etag)
{
	sync_task_ptr task(new file_uploader(data, path, remote, etag));
	agenda_ptr ptr = shared_from_this();
	the_pool.schedule(task_executor {task, ptr});
}

void agenda::schedule_removal(const connection_data &data,
							  remote_file_ptr file)
{
	sync_task_ptr task(new file_deleter(data, file->full_name_));
	agenda_ptr ptr = shared_from_this();
	the_pool.schedule(task_executor {task, ptr});
}

void agenda::schedule(sync_task_ptr task)
{
	agenda_ptr ptr = shared_from_this();
	the_pool.schedule(task_executor {task, ptr});
}
