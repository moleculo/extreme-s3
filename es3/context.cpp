#include "context.h"
#include "scope_guard.h"
#include "errors.h"

using namespace boost::filesystem;

#define SEGMENT_SIZE (6*1024*1024)
#define MIN_SEGMENT_SIZE (6*1024*1024)
#define MAX_IN_FLIGHT 200

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
			parent_->condition_.notify_all();
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
	in_flight_(), do_compression_(), use_ssl_()
{
	max_in_flight_=MAX_IN_FLIGHT/2;
	max_readers_=sysconf(_SC_NPROCESSORS_ONLN)+1;
	segment_size_=SEGMENT_SIZE;
	max_compressors_=sysconf(_SC_NPROCESSORS_ONLN)+1;
}

void conn_context::validate()
{
	if (max_in_flight_>MAX_IN_FLIGHT || max_in_flight_<=0)
		max_in_flight_=MAX_IN_FLIGHT;
	if (max_readers_<=0)
		max_readers_=sysconf(_SC_NPROCESSORS_ONLN)+1;
	if (segment_size_<MIN_SEGMENT_SIZE)
		segment_size_=MIN_SEGMENT_SIZE;
	if (max_compressors_<=0)
		max_compressors_=sysconf(_SC_NPROCESSORS_ONLN)*2;
}
