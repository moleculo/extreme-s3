PROJECT(es3)

SET(es3_SRCS
	agenda.cpp
	base64.cpp
	common.cpp
	connection.cpp
	errors.cpp
	main.cpp

	uploader.cpp
	sync.cpp
)
SET(es3_INCLUDES
	agenda.h
	common.h
	connection.h
	errors.h
	scope_guard.h
	uploader.h
	sync.h
)

INCLUDE_DIRECTORIES(.)

ADD_EXECUTABLE(es3 ${es3_SRCS} ${es3_INCLUDES})
TARGET_LINK_LIBRARIES(es3 boost_program_options
	boost_filesystem boost_system curl crypto tinyxml)
