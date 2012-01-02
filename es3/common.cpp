#include "common.h"
#include <sstream>
#include "errors.h"
#include <stdio.h>

#define BOOST_KARMA_NUMERICS_LOOP_UNROLL 6
#include <boost/spirit/include/karma.hpp>
using namespace boost::spirit;
using boost::spirit::karma::int_;
using boost::spirit::karma::lit;

using namespace es3;

static int global_verbosity_level = 0;
std::mutex logger_lock_;

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

void es3::append_int_to_string(int64_t in, std::string &out)
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

std::string es3::format_time(time_t time)
{
	struct tm * timeinfo = gmtime(&time);
	char date_header[80] = {0};
	strftime(date_header, 80, "%a, %d %b %Y %H:%M:%S +0000", timeinfo);
	return date_header;
}

handle_t::handle_t()
{
	fileno_ = 0;
}

handle_t::handle_t(int fileno)
{
	fileno_ = fileno | libc_die;
}

handle_t::handle_t(const handle_t& other)
{
	fileno_ = ::dup(other.fileno_) | libc_die;
}

handle_t& handle_t::operator = (const handle_t &other)
{
	if (this==&other) return *this;
	if (fileno_) close(fileno_);
	fileno_ = ::dup(other.fileno_) | libc_die;
	return *this;
}

handle_t handle_t::dup() const
{
	if (!fileno_)
		return handle_t();
	return handle_t(::dup(fileno_) | libc_die);
}

handle_t::~handle_t()
{
	if (fileno_)
		close(fileno_);
}
