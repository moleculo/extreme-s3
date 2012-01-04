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
	s<<("Error code: ")<<code.code()<<", description: '"<<code.desc()<<"'";
	s.flush();
	what_=s.str();
}

void es3::throw_libc_err()
{
	char buf[1024]={0};
	strerror_r(errno, buf, 1023);
	err(errFatal) << "Got error: " << std::string(buf);
	//Unreachable
}
