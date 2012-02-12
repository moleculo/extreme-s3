#ifndef AGENDA_H
#define AGENDA_H

#include "common.h"
#include <boost/enable_shared_from_this.hpp>
#include <condition_variable>

#define MIN_SEGMENT_SIZE (6*1024*1024)
#define MAX_IN_FLIGHT 200

namespace es3 {
	class agenda;
	typedef boost::shared_ptr<agenda> agenda_ptr;

	struct segment
	{
		std::vector<char> data_;
	};
	typedef boost::shared_ptr<segment> segment_ptr;

	enum task_type_e
	{
		taskUnbound,
		taskCPUBound,
		taskIOBound,
	};

	class sync_task
	{
	public:
		virtual ~sync_task() {}

		virtual task_type_e get_class() const { return taskUnbound; }
		virtual size_t needs_segments() const { return 0; }
		virtual int64_t ordinal() const
		{
			return 0;
		}
		virtual void operator()(agenda_ptr agenda,
								const std::vector<segment_ptr> &segments)
		{
			operator ()(agenda);
		}
		virtual void operator()(agenda_ptr agenda){}
		virtual void print_to(std::ostream &str) = 0;
	};
	typedef boost::shared_ptr<sync_task> sync_task_ptr;

	inline bool operator < (sync_task_ptr p1, sync_task_ptr p2)
	{
		return p1->ordinal() < p2->ordinal();
	}

	class agenda : public boost::enable_shared_from_this<agenda>
	{
		const std::map<task_type_e, size_t> class_limits_;
		const size_t max_segments_in_flight_, segment_size_;
		const bool quiet_, final_quiet_;
		struct timespec start_time_;

		std::mutex m_; //This mutex protects the following data {
		std::condition_variable condition_;
		typedef std::multimap<int64_t, sync_task_ptr> task_map_t;
		std::map<size_t, std::map<task_type_e, task_map_t> > tasks_;
		std::map<task_type_e, size_t> classes_;
		size_t num_working_;
		size_t segments_in_flight_;
		//}

		std::mutex stats_m_; //This mutex protects the following data {
		size_t num_submitted_, num_done_, num_failed_;
		std::map<std::string, std::pair<uint64_t, uint64_t> > progress_;
		std::map<std::string, uint64_t> cur_stats_;
		//}

		friend struct segment_deleter;
	public:
		agenda(size_t num_unbound, size_t num_cpu_bound,
			   size_t num_io_bound, bool quiet, bool final_quiet,
			   size_t def_segment_size, size_t max_segments_in_flight_);

		size_t get_capability(task_type_e tp) const
		{
			return class_limits_.at(tp);
		}
		void schedule(sync_task_ptr task);
		size_t run();

		void add_stat_counter(const std::string &stat, uint64_t val);

		size_t segment_size() const { return segment_size_; }

		void print_queue();
		void print_epilog();
		size_t tasks_count() const { return tasks_.size(); }
	private:
		segment_ptr get_segment();

		void draw_progress();
		void draw_progress_widget();
		void draw_stats();
		uint64_t get_elapsed_millis() const;
		std::pair<std::string, std::string> format_si(uint64_t val,
													  bool per_sec);

		friend class task_executor;
	};

}; //namespace es3

#endif //AGENDA_H
