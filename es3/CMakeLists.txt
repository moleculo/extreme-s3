PROJECT(es3)

SET(es3_SRCS
	agenda.cpp
	common.cpp
	connection.cpp
	errors.cpp
	main.cpp
)
SET(es3_INCLUDES
	agenda.h
	common.h
	connection.h
	errors.h
	scope_guard.h
)

INCLUDE_DIRECTORIES(.)

ADD_EXECUTABLE(es3 ${es3_SRCS} ${es3_INCLUDES})
TARGET_LINK_LIBRARIES(es3 pthread gflags glog boost_filesystem curl)
