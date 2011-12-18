#include "agenda.h"

#include <unistd.h>
#include <boost/threadpool.hpp>
#include <functional>

using namespace es3;

//Utility thread pool
boost::threadpool::pool the_pool(sysconf(_SC_NPROCESSORS_ONLN));

agenda_ptr agenda::make_new()
{
	return agenda_ptr(new agenda());
}

agenda::agenda()
{

}

void agenda::schedule(sync_task_ptr task)
{

}
