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
	std::string lvl;
	if (code.code()==errFatal)
		lvl="ERROR";
	else if (code.code()==errWarn)
		lvl="WARN";
	else
		lvl="INFO";
	s<<lvl<<": '"<<code_.desc()<<"'";
	s.flush();
	what_=s.str();

	//Backtrace exception
	backtrace_it();
}

void es3::throw_libc_err(const std::string &desc)
{
	int cur_err = errno;
	if (cur_err==0)
		return; //No error - we might be here accidentally
//	char buf[1024]={0};
//	strerror_r(cur_err, buf, 1023);
	err(errFatal) << "Error code " << cur_err << ". " << desc;
}
