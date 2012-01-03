#include "context.h"
#include <unistd.h>

using namespace es3;

namespace es3 {
	struct segment_deleter
	{
		context_ptr parent_;

		void operator()(segment *seg)
		{
			delete seg;

			u_guard_t guard(parent_->m_);
			parent_->in_flight_--;
			parent_->condition_.notify_one();
		}
	};

}; //namespace es3

segment_ptr conn_context::get_segment()
{
	u_guard_t guard(m_);
	while(in_flight_>MAX_IN_FLIGHT)
		condition_.wait(guard);

	segment_deleter del {shared_from_this()};
	segment_ptr res=segment_ptr(new segment(), del);
	in_flight_++;
	return res;
}

conn_context::conn_context() :
	in_flight_()
{
	max_in_flight_=MAX_IN_FLIGHT;
	max_compressors_=sysconf(_SC_NPROCESSORS_ONLN)+1;
}
