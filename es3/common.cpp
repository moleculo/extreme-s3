#include "common.h"
#include <sstream>

#define BOOST_KARMA_NUMERICS_LOOP_UNROLL 6
#include <boost/spirit/include/karma.hpp>
using namespace boost::spirit;
using boost::spirit::karma::int_;
using boost::spirit::karma::lit;

using namespace es3;

static int global_verbosity_level = 0;
std::recursive_mutex logger_lock_;

es3::logger::logger(int lvl)
	: verbosity_(lvl), stream_(new std::ostringstream())
{
}

es3::logger::~logger()
{
	guard_t g(logger_lock_);
	std::cerr
			<< static_cast<std::ostringstream*>(stream_.get())->str()
			<< std::endl;
}

void es3::logger::set_verbosity(int lvl)
{
	global_verbosity_level = lvl;
}

bool es3::logger::is_log_on(int lvl)
{
	return lvl<=global_verbosity_level;
}

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

std::string es3::trim(const std::string &str)
{
		std::string res;
		bool hit_non_ws=false;
		int ws_span=0;
		for(std::string::const_iterator iter=str.begin();iter!=str.end();++iter)
		{
				const char c=*iter;

				if (c==' ' || c=='\n' || c=='\r' || c=='\t')
				{
						ws_span++;
				} else
				{
						if (ws_span!=0 && hit_non_ws)
								res.append(ws_span, ' ');
						ws_span=0;
						hit_non_ws=true;

						res.append(1, c);
				}
		}
		return res;
}

std::string es3::tobinhex(const unsigned char* data, size_t ln)
{
	static const char alphabet[17]="0123456789abcdef";
	std::string res;
	res.resize(ln*2);
	for(size_t f=0;f<ln;++f)
	{
		res[f*2]=alphabet[data[f]/16];
		res[f*2+1]=alphabet[data[f]%16];
	}
	return res;
}
