#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#define MAX_SEGMENTS 9999

namespace es3 {

	class conn_context : public boost::enable_shared_from_this<conn_context>
	{
	public:
		bf::path scratch_dir_;
		bool use_ssl_, do_compression_;
		std::string api_key_, secret_key;

		conn_context() : use_ssl_(), do_compression_(true) {};
	private:
		conn_context(const conn_context &);
	};
	typedef boost::shared_ptr<conn_context> context_ptr;
}; //namespace es3

#endif //CONTEXT_H
