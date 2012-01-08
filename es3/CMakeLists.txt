PROJECT(es3)

SET(es3_SRCS
	agenda.cpp
	base64.cpp
	common.cpp
	compressor.cpp
	connection.cpp
	downloader.cpp
	errors.cpp
	main.cpp

	uploader.cpp
	sync.cpp
)
SET(es3_INCLUDES
	agenda.h
	common.h
	compressor.h
	connection.h
	context.h
	downloader.h
	errors.h
	scope_guard.h
	uploader.h
	sync.h
)

SET(Boost_USE_STATIC_LIBS ON)
SET(Boost_USE_STATIC_RUNTIME ON)
SET(Boost_USE_STATIC_RUNTIME ON)
SET(Boost_ADDITIONAL_VERSIONS "1.42" "1.42.0" "1.48" "1.48.0" "1.46" "1.46.1")
FIND_PACKAGE(Boost 1.42 COMPONENTS program_options filesystem system)
FIND_PACKAGE(CURL)
FIND_PACKAGE(OpenSSL)

FIND_PATH(TINYXML_INCLUDE_DIR NAMES tinyxml.h)
FIND_LIBRARY(TINYXML_LIBRARY NAMES libtinyxml.a tinyxml.lib libtinyxml.so)

INCLUDE_DIRECTORIES(.)
INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${TINYXML_INCLUDE_DIR})

ADD_EXECUTABLE(es3 ${es3_SRCS} ${es3_INCLUDES})
TARGET_LINK_LIBRARIES(es3
	${Boost_LIBRARIES} ${CURL_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARY}
	${TINYXML_LIBRARY})
