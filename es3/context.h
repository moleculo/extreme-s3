#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#define MAX_SEGMENTS 9999

typedef void CURL;

namespace es3 {
	struct s3_path;

	typedef boost::shared_ptr<CURL> curl_ptr_t;

	class conn_context : public boost::enable_shared_from_this<conn_context>
	{
	public:
		bf::path scratch_dir_;
		bool use_ssl_, do_compression_;
		std::string api_key_, secret_key;

		conn_context() : use_ssl_(), do_compression_(true) {};
		~conn_context();

		curl_ptr_t get_curl(const std::string &zone,
					   const std::string &bucket);
		void reset();
		char* err_buf_for(curl_ptr_t ptr)
		{
			return error_bufs_[ptr.get()];
		}

	private:
		conn_context(const conn_context &);

		void release_curl(CURL*);
		std::mutex m_;
		std::map<std::string, std::vector<CURL*> > curls_;
		std::map<CURL*, char*> error_bufs_;
		std::map<CURL*, std::string> borrowed_curls_;

		friend class curl_deleter;
	};
	typedef boost::shared_ptr<conn_context> context_ptr;
}; //namespace es3

#endif //CONTEXT_H
