#include "agenda.h"
#include "errors.h"
#include <unistd.h>
#include <thread>
#include <sstream>
#include <unistd.h>
#include <boost/bind.hpp>
#include <time.h>
#include <iostream>

using namespace es3;

agenda::agenda(size_t num_unbound, size_t num_cpu_bound, size_t num_io_bound,
			   bool quiet, bool final_quiet,
			   size_t segment_size, size_t max_segments_in_flight) :
	class_limits_ {{taskUnbound, num_unbound},
				   {taskCPUBound, num_cpu_bound},
				   {taskIOBound, num_io_bound}},
	quiet_(quiet), final_quiet_(final_quiet), segment_size_(segment_size),
	max_segments_in_flight_(max_segments_in_flight),
	num_working_(), num_submitted_(), num_done_(), num_failed_(),
	segments_in_flight_()
{
	clock_gettime(CLOCK_MONOTONIC, &start_time_) | libc_die2("Can't get time");
}

namespace es3
{
	struct segment_deleter
	{
		agenda_ptr parent_;

		void operator()(segment *seg)
		{
			delete seg;

			u_guard_t guard(parent_->m_);
			assert(parent_->segments_in_flight_>0);
			parent_->segments_in_flight_--;
			parent_->condition_.notify_one();
			printf("Released\n");
		}
	};

	class task_executor
	{
		agenda_ptr agenda_;
	public:
		task_executor(agenda_ptr agenda) : agenda_(agenda) {}

		std::pair<sync_task_ptr, std::vector<segment_ptr> > claim_task()
		{
			std::pair<sync_task_ptr, std::vector<segment_ptr> > res_pair;
			while(true)
			{
				u_guard_t lock(agenda_->m_);
				if (agenda_->tasks_.empty())
				{
					if (agenda_->num_working_==0)
						return res_pair;
					agenda_->condition_.wait(lock);
					continue;
				}

				//Iterate over classes to find one that is not yet full
				for(auto iter=agenda_->classes_.begin();
					iter!=agenda_->classes_.end();++iter)
				{
					//Check if there are too many tasks of this type running
					task_type_e cur_class=iter->first;

					size_t cur_num=iter->second;
					size_t limit=agenda_->get_capability(cur_class);
					//Unbound tasks are allowed to exceed their limits and
					//borrow threads from other classes
//					if (cur_class!=taskUnbound && limit<=cur_num)
//						continue; //Too busy
					if (limit<=cur_num)
						continue;

					//Good! We can work on this class.
					//Find the task with the greatest segment requirements
					auto pair=agenda_->tasks_.rbegin();
					size_t segments_needed=pair->first;
					size_t segments_avail=agenda_->max_segments_in_flight_-
							agenda_->segments_in_flight_;
					if (segments_needed>segments_avail)
					{
						printf("Segs\n");
						continue; //No such luck :(
					}

					if (!pair->second.count(cur_class))
						continue; //No tasks for this class

					agenda::task_map_t &task_map=pair->second.at(cur_class);
					assert(!task_map.empty());
					sync_task_ptr res=task_map.begin()->second;
					task_map.erase(task_map.begin());
					if (task_map.empty())
					{
						pair->second.erase(cur_class);
						if (pair->second.empty())
							agenda_->tasks_.erase(pair->first);
					}

					agenda_->num_working_++;
					agenda_->classes_[cur_class]++;

					res_pair.first=res;
					if (segments_needed)
						res_pair.second=agenda_->get_segments(segments_needed);
					return res_pair;
				}

				agenda_->condition_.wait(lock);
			}
		}

		void cleanup(sync_task_ptr cur_task, bool fail)
		{
			u_guard_t lock(agenda_->m_);
			agenda_->num_working_--;
			assert(agenda_->classes_[cur_task->get_class()]>0);
			agenda_->classes_[cur_task->get_class()]--;

			if (agenda_->tasks_.empty() && agenda_->num_working_==0)
				agenda_->condition_.notify_all(); //We've finished our tasks!
			else
				agenda_->condition_.notify_one();

			//Update stats
			guard_t lockst(agenda_->stats_m_);
			agenda_->num_done_++;
			if (fail)
				agenda_->num_failed_++;
		}

		void operator ()()
		{
			while(true)
			{
				std::pair<sync_task_ptr, std::vector<segment_ptr> > cur_task;
				cur_task=claim_task();
				if (!cur_task.first)
					break;

				bool fail=true;
				for(int f=0; f<10; ++f)
				{
					try
					{
						(*cur_task.first)(agenda_, cur_task.second);
						fail=false;
						break;
					} catch (const es3_exception &ex)
					{
						const result_code_t &code = ex.err();
						if (code.code()==errNone)
						{
							VLOG(2) << "INFO: " << ex.what();
							sleep(5);
							continue;
						} else if (code.code()==errWarn)
						{
							VLOG(1) << "WARN: " << ex.what();
							sleep(5);
							continue;
						} else
						{
							VLOG(0) << ex.what();
							break;
						}
					} catch(const std::exception &ex)
					{
						VLOG(0) << "ERR: " << ex.what();
						break;
					} catch(...)
					{
						VLOG(0) << "Unknown exception. Skipping";
						break;
					}
				}

				cleanup(cur_task.first, fail);
			}
		}
	};
}

std::vector<segment_ptr> agenda::get_segments(size_t num)
{
	assert(segments_in_flight_+num<=max_segments_in_flight_);

	std::vector<segment_ptr> res;
	res.reserve(num);
	for(size_t f=0;f<num;++f)
	{
		segment_deleter del {shared_from_this()};
		segment_ptr seg=segment_ptr(new segment(), del);
		res.push_back(seg);
	}
	segments_in_flight_+=num;
	return res;
}

void agenda::schedule(sync_task_ptr task)
{
	u_guard_t lock(m_);
//	agenda_ptr ptr = shared_from_this();

//	auto iter=std::lower_bound(tasks_.begin(), tasks_.end(), task);
//	if (iter!=tasks_.end())
//		tasks_.insert(iter, task);
//	else
//		tasks_.push_back(task);
	classes_[task->get_class()]; //Force insertion of class entry
	task_map_t &task_map=tasks_[task->needs_segments()][task->get_class()];
	task_map.insert(std::make_pair(task->ordinal(), task));
	condition_.notify_one();

	guard_t lockst(stats_m_);
	num_submitted_++;
}

size_t agenda::run()
{
	std::vector<std::thread> threads;
	size_t thread_num=0;
	for(auto iter=class_limits_.begin();iter!=class_limits_.end();++iter)
		thread_num+=iter->second;

	for(int f=0;f<thread_num;++f)
		threads.push_back(std::thread(task_executor(shared_from_this())));

	if (!quiet_)
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
		for(int f=0;f<threads.size();++f)
			threads.at(f).join();
	}

	return num_failed_;
}

void agenda::print_epilog()
{
	if (!final_quiet_)
		draw_stats();
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

void agenda::add_stat_counter(const std::string &stat, uint64_t val)
{
	guard_t lockst(stats_m_);
	cur_stats_[stat]+=val;
}

std::pair<std::string, std::string> agenda::format_si(uint64_t val,
													  bool per_sec)
{
	std::pair<std::string, std::string> res;
	uint64_t denom = 1;
	if (val<10000)
	{
		denom = 1;
		res.second = "B";
	} else if (val<2000000)
	{
		denom = 1024;
		res.second = "K";
	} else
	{
		denom = 1024*1024;
		res.second = "M";
	}
	res.first = int_to_string(val/denom);
	if (val%denom)
		res.first+="."+int_to_string((val%denom)*100/denom);
	return res;
}

void agenda::draw_progress_widget()
{
	uint64_t el = get_elapsed_millis();

	std::stringstream str;
	{
		guard_t lockst(stats_m_);
		str << "Tasks: [" << num_done_ << "/" << num_submitted_
			<< "]";
		if (num_failed_)
			str << " Failed tasks: " << num_failed_;

		uint64_t uploaded = cur_stats_["uploaded"];
		uint64_t downloaded = cur_stats_["downloaded"];
		if (downloaded)
		{
			auto dl=format_si(downloaded, false);
			auto ds=format_si(el==0? 0 : (downloaded*1000/el), true);
			str << "  Downloaded: "
				<< dl.first << " " << dl.second << ", speed: "
				<< ds.first << " " << ds.second << "/sec";
		}
		if (uploaded)
		{
			auto ul=format_si(uploaded, false);
			auto us=format_si(el==0? 0 : (uploaded*1000/el), true);
			str << "  Uploaded: "
				<< ul.first << " " << ul.second << ", speed: "
				<< us.first << " " << us.second << "/sec";
		}

		str << "\r";
	}

	std::cerr << str.str(); //No std::endl
	std::cerr.flush();
}

uint64_t agenda::get_elapsed_millis() const
{
	struct timespec cur;
	clock_gettime(CLOCK_MONOTONIC, &cur) | libc_die2("Can't get time");
	uint64_t start_tm = uint64_t(start_time_.tv_sec)*1000 +
			start_time_.tv_nsec/1000000;

	uint64_t cur_tm = uint64_t(cur.tv_sec)*1000+cur.tv_nsec/1000000;
	return cur_tm-start_tm;
}

void agenda::draw_stats()
{
	uint64_t el = get_elapsed_millis();

	std::cerr << "time taken [sec]: " << el/1000 << "." << el%1000 << std::endl;
	for(auto f=cur_stats_.begin();f!=cur_stats_.end();++f)
	{
		std::string name=f->first;
		uint64_t val=f->second;
		if (!val) continue;

		uint64_t avg = val*1000/el;

		std::cerr << name << " [B]: " << val
				  << ", average [B/sec]: " << avg
				  << std::endl;
	}
}

void agenda::print_queue()
{
	std::cerr << "There are " << tasks_.size() << " task[s] present.\n";
	for(auto by_segs=tasks_.begin();by_segs!=tasks_.end();++by_segs)
	{
		for(auto iter=by_segs->second.begin();
			iter!=by_segs->second.end(); ++iter)
		{
			for(auto iter2=iter->second.begin();
				iter2!=iter->second.end();++iter2)
			{
				sync_task_ptr task=iter2->second;
				task->print_to(std::cerr);
				std::cerr<<std::endl;
			}
		}
	}
}
