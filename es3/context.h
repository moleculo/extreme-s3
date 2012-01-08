#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include <condition_variable>
#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem.hpp>

#define MAX_SEGMENTS 9999

namespace es3 {
	namespace bf = boost::filesystem3;

	class conn_context : public boost::enable_shared_from_this<conn_context>
	{
	public:
		bool use_ssl_, do_compression_;
		std::string api_key_, secret_key;
		std::string zone_, bucket_;

		conn_context();
	private:
		conn_context(const conn_context &);
	};
	typedef boost::shared_ptr<conn_context> context_ptr;
}; //namespace es3

#endif //CONTEXT_H
