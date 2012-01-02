#include "errors.h"
#include <sstream>
#include <string.h>

using namespace es3;

extern ES3LIB_PUBLIC const die_t es3::die=die_t();
extern ES3LIB_PUBLIC const libc_die_t es3::libc_die=libc_die_t();
extern ES3LIB_PUBLIC const result_code_t es3::sok=result_code_t();

es3_exception::es3_exception(const result_code_t &code) : code_(code)
{
	std::stringstream s;
	s<<("Error code: ")<<code.code()<<", description: "<<code.desc();
	what_=s.str();
}

int es3::operator | (const int res, const libc_die_t &)
{
	if (res>=0)
		return res;
	char buf[1024]={0};

	strerror_r(errno, buf, 1023);
	err(errFatal) << "Got error: " << buf;
	//Unreachable
}
