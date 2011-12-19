PROJECT(es3)

SET(es3_SRCS
	agenda.cpp
	base64.cpp
	common.cpp
	connection.cpp
	errors.cpp
	main.cpp

	uploader.cpp
)
SET(es3_INCLUDES
	agenda.h
	common.h
	connection.h
	errors.h
	scope_guard.h
	uploader.h

	boost/threadpool/shutdown_policies.hpp
	boost/threadpool/detail/future.hpp
	boost/threadpool/detail/locking_ptr.hpp
	boost/threadpool/detail/worker_thread.hpp
	boost/threadpool/detail/scope_guard.hpp
	boost/threadpool/detail/pool_core.hpp
	boost/threadpool/pool.hpp
	boost/threadpool/task_adaptors.hpp
	boost/threadpool/pool_adaptors.hpp
	boost/threadpool/scheduling_policies.hpp
	boost/threadpool/future.hpp
	boost/threadpool/size_policies.hpp
	boost/threadpool.hpp
)

INCLUDE_DIRECTORIES(.)

ADD_EXECUTABLE(es3 ${es3_SRCS} ${es3_INCLUDES})
TARGET_LINK_LIBRARIES(es3 pthread gflags glog boost_thread
	boost_filesystem boost_system curl ssl crypto tinyxml)
