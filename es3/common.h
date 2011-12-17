#ifndef COMMON_H
#define COMMON_H

#if defined _WIN32 || defined __CYGWIN__
  #ifdef LIBSOFADB_EXPORTS
	#ifdef __GNUC__
	  #define SOFADB_PUBLIC __attribute__ ((dllexport))
	#else
	  #define SOFADB_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
  #else
	#ifdef __GNUC__
	  #define SOFADB_PUBLIC __attribute__ ((dllimport))
	#else
	  #define SOFADB_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
  #endif
  #define SOFADB_LOCAL
#else
  #if __GNUC__ >= 4
	#define SOFADB_PUBLIC __attribute__ ((visibility ("default")))
	#define SOFADB_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
	#define SOFADB_PUBLIC
	#define SOFADB_LOCAL
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

namespace sofadb {
	SOFADB_PUBLIC std::string int_to_string(int64_t in);
	SOFADB_PUBLIC void append_int_to_string(int64_t in, jstring_t &out);
};

#endif // COMMON_H
