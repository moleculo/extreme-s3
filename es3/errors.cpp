#include "errors.h"
#include <sstream>

using namespace es3;

extern ES3LIB_PUBLIC const die_t es3::die=die_t();
extern ES3LIB_PUBLIC const result_code_t es3::sok=result_code_t();

es3_exception::es3_exception(const result_code_t &code) : code_(code)
{
	std::stringstream s;
	s<<("Error code: ")<<code.code()<<", description: "<<code.desc();
	what_=s.str();
}
