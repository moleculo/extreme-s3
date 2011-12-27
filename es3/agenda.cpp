#include "agenda.h"
#include "errors.h"
#include <condition_variable>
#include <unistd.h>
#include <thread>

using namespace es3;

agenda_ptr agenda::make_new(size_t thread_num)
{
	return agenda_ptr(new agenda(thread_num));
}

agenda::agenda(size_t thread_num) : num_working_(), thread_num_(thread_num)
{

}

namespace es3
{
	class task_executor
	{
		agenda_ptr agenda_;
	public:
		task_executor(agenda_ptr agenda) : agenda_(agenda) {}

		sync_task_ptr claim_task()
		{
			while(true)
			{
				u_guard_t lock(agenda_->m_);
				if (agenda_->tasks_.empty())
				{
					if (agenda_->num_working_==0)
						return sync_task_ptr();
					else
					{
						agenda_->condition_.wait(lock);
						continue;
					}
				}

				sync_task_ptr cur_task = agenda_->tasks_.back();
				agenda_->tasks_.pop_back();
				agenda_->num_working_++;
				return cur_task;
			}
		}

		void cleanup()
		{
			u_guard_t lock(agenda_->m_);
			agenda_->num_working_--;
			if (agenda_->tasks_.empty() && agenda_->num_working_==0)
				agenda_->condition_.notify_all();
		}

		void operator ()()
		{
			while(true)
			{
				sync_task_ptr cur_task=claim_task();
				if (!cur_task)
					break;

				for(int f=0; f<10; ++f)
				{
					try
					{
						(*cur_task)(agenda_);
						break;
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
					} catch(...)
					{
						cleanup();
						throw;
					}
				}

				cleanup();
			}
		}
	};
}

void agenda::schedule(sync_task_ptr task)
{
	u_guard_t lock(m_);
	agenda_ptr ptr = shared_from_this();
	tasks_.push_back(task);
	condition_.notify_one();
}

void agenda::run()
{
	std::vector<std::thread> threads;
	size_t num_threads = thread_num_>0 ? thread_num_ :
			sysconf(_SC_NPROCESSORS_ONLN)+8;

	for(int f=0;f<num_threads;++f)
		threads.push_back(std::thread(task_executor(shared_from_this())));

	for(int f=0;f<num_threads;++f)
		threads.at(f).join();
}
