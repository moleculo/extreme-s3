#include "agenda.h"
#include "errors.h"
#include <unistd.h>
#include <thread>
#include <sstream>
#include <unistd.h>
#include <boost/bind.hpp>

using namespace es3;

agenda_ptr agenda::make_new(size_t thread_num)
{
	return agenda_ptr(new agenda(thread_num));
}

agenda::agenda(size_t thread_num) : num_working_(),
	num_submitted_(), num_done_()
{
	thread_num_ = thread_num>0 ? thread_num : sysconf(_SC_NPROCESSORS_ONLN)+1;
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
				}

				for(auto iter=agenda_->tasks_.begin();
					iter!=agenda_->tasks_.end();++iter)
				{
					sync_task_ptr cur_task = *iter;
					//Check if there are too many tasks of this type running
					int cur_num=agenda_->classes_[cur_task->get_class()];
					if (cur_task->get_class_limit()!=-1 &&
							cur_task->get_class_limit()<=cur_num)
						continue;

					agenda_->tasks_.erase(iter);
					agenda_->num_working_++;
					agenda_->classes_[cur_task->get_class()]++;
					return cur_task;
				}

				agenda_->condition_.wait(lock);
			}
		}

		void cleanup(sync_task_ptr cur_task)
		{
			u_guard_t lock(agenda_->m_);
			agenda_->num_working_--;
			agenda_->classes_[cur_task->get_class()]--;
			if (agenda_->tasks_.empty() && agenda_->num_working_==0 ||
					cur_task->get_class_limit()!=-1)
				agenda_->condition_.notify_all();

			guard_t lockst(agenda_->stats_m_);
			agenda_->num_done_++;
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
							VLOG(1) << "INFO: " << ex.what();
							continue;
						} else if (code.code()==errWarn)
						{
							VLOG(0) << "WARN: " << ex.what();
							continue;
						} else
						{
							VLOG(0) << ex.what();
							break;
						}
					} catch(...)
					{
						cleanup(cur_task);
						throw;
					}
				}

				cleanup(cur_task);
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

	guard_t lockst(stats_m_);
	num_submitted_++;
}

void agenda::run(bool visual)
{
	std::vector<std::thread> threads;
	for(int f=0;f<thread_num_;++f)
		threads.push_back(std::thread(task_executor(shared_from_this())));

	if (visual)
	{
		threads.push_back(std::thread(boost::bind(&agenda::draw_progress,
												  this)));
		for(int f=0;f<threads.size();++f)
			threads.at(f).join();

		//Draw progress the last time
		draw_progress_widget();
		std::cerr<<std::endl;
	} else
	{
		for(int f=0;f<thread_num_;++f)
			threads.at(f).join();
	}
}

void agenda::draw_progress()
{
	while(true)
	{
		{
			guard_t g(m_);
			if (num_working_==0 && tasks_.empty())
				return;
		}
		draw_progress_widget();
		usleep(500000);
	}
}

void agenda::draw_progress_widget()
{
	std::stringstream str;
	{
		guard_t lockst(stats_m_);
		str << "Tasks: [" << num_done_ << "/" << num_submitted_
			<< "]" << "\r";
	}

	std::cerr << str.str(); //No std::endl
	std::cerr.flush();
}
