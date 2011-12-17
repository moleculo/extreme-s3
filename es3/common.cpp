#include "common.h"

#define BOOST_KARMA_NUMERICS_LOOP_UNROLL 6
#include <boost/spirit/include/karma.hpp>
using namespace boost::spirit;
using boost::spirit::karma::int_;
using boost::spirit::karma::lit;

using namespace es3;

std::string es3::int_to_string(int64_t in)
{
		char buffer[64];
		char *ptr = buffer;
		karma::generate(ptr, int_, in);
		*ptr = '\0';
		return std::string(buffer, ptr-buffer);
}

void es3::append_int_to_string(int64_t in, jstring_t &out)
{
		char buffer[64];
		char *ptr = buffer;
		karma::generate(ptr, int_, in);
		*ptr = '\0';
		out.append(buffer, ptr);
}
