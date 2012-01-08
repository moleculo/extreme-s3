#include "context.h"
#include "scope_guard.h"
#include "errors.h"

using namespace boost::filesystem;

using namespace es3;

conn_context::conn_context() :
	do_compression_(), use_ssl_()
{
}

//#define SEGMENT_SIZE (6*1024*1024)
//#define MIN_SEGMENT_SIZE (6*1024*1024)
//#define MAX_IN_FLIGHT 200

//void conn_context::validate()
//{
//	if (max_in_flight_>MAX_IN_FLIGHT || max_in_flight_<=0)
//		max_in_flight_=MAX_IN_FLIGHT;
//	if (max_readers_<=0)
//		max_readers_=sysconf(_SC_NPROCESSORS_ONLN)+1;
//	if (segment_size_<MIN_SEGMENT_SIZE)
//		segment_size_=MIN_SEGMENT_SIZE;
//	if (max_compressors_<=0)
//		max_compressors_=sysconf(_SC_NPROCESSORS_ONLN)*2;
//}
