#ifndef AGENDA_H
#define AGENDA_H

#include "common.h"
#include "errors.h"

namespace es3 {

	class sync_task
	{
	public:
		virtual ~sync_task() {}
		virtual result_code_t operator()() = 0;
		virtual std::string describe()=0;
	};
	typedef boost::shared_ptr<sync_task> sync_task_ptr;

	class agenda : public boost::enable_shared_from_this<agenda>
	{
		agenda();
		std::vector<sync_task_ptr> scheduled_;

		std::vector< std::pair<sync_task_ptr, result_code_t> > results_;
	public:
		static boost::shared_ptr<agenda> make_new();

	};

	typedef boost::shared_ptr<agenda> agenda_ptr;

}; //namespace es3

#endif //AGENDA_H
