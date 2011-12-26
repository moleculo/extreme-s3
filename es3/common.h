#ifndef COMMON_H
#define COMMON_H

#if defined _WIN32 || defined __CYGWIN__
  #ifdef LIBES3LIB_EXPORTS
	#ifdef __GNUC__
	  #define ES3LIB_PUBLIC __attribute__ ((dllexport))
	#else
	  #define ES3LIB_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
  #else
	#ifdef __GNUC__
	  #define ES3LIB_PUBLIC __attribute__ ((dllimport))
	#else
	  #define ES3LIB_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
  #endif
  #define ES3LIB_LOCAL
#else
  #if __GNUC__ >= 4
	#define ES3LIB_PUBLIC __attribute__ ((visibility ("default")))
	#define ES3LIB_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
	#define ES3LIB_PUBLIC
	#define ES3LIB_LOCAL
  #endif
#endif

#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>
#include <stdexcept>

#include <mutex>
typedef std::lock_guard<std::recursive_mutex> guard_t;

#include <glog/logging.h>
#define VLOG_MACRO(lev) if(VLOG_IS_ON(lev)) VLOG(lev)

#define BOOST_SYSTEM_NO_DEPRECATED
#undef BOOST_HAS_RVALUE_REFS
#include <boost/thread.hpp>
#define BOOST_HAS_RVALUE_REFS

//#include <ext/vstring.h>
//typedef __gnu_cxx::__vstring jstring_t;
typedef std::string jstring_t;

#ifndef NDEBUG
	#include <execinfo.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <unistd.h>

	inline void backtrace_it(void)
	{
		void *buffer[1000];
		int nptrs = backtrace(buffer, 1000);
		printf("backtrace() returned %d addresses\n", nptrs);
		backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO);
	}
#endif

namespace es3 {
	ES3LIB_PUBLIC std::string int_to_string(int64_t in);
	ES3LIB_PUBLIC void append_int_to_string(int64_t in, jstring_t &out);

	ES3LIB_PUBLIC std::string base64_encode(const char *, size_t len);
	ES3LIB_PUBLIC std::string base64_decode(const std::string &s);
	ES3LIB_PUBLIC std::string trim(const std::string &str);

	ES3LIB_PUBLIC std::string tobinhex(const unsigned char* data, size_t ln);
};

#endif // COMMON_H
