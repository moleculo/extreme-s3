#include "context.h"
#include <curl/curl.h>
#include "errors.h"

using namespace es3;

namespace es3
{
	struct curl_deleter
	{
		conn_context *parent_;

		void operator()(CURL *curl)
		{
			parent_->release_curl(curl);
		}
	};
};

void conn_context::reset()
{
	assert(borrowed_curls_.empty());
	for(auto iter=curls_.begin();iter!=curls_.end();++iter)
		for(auto citer=iter->second.begin();citer!=iter->second.end();++citer)
		{
			curl_easy_cleanup(*citer);
			assert(error_bufs_.count(*citer));
			free(error_bufs_.at(*citer));
		}
	curls_.clear();
	error_bufs_.clear();
}

conn_context::~conn_context()
{
	reset();
}

curl_ptr_t conn_context::get_curl(const std::string &zone,
							 const std::string &bucket)
{
	guard_t lock(m_);
	std::vector<CURL*> &cur = curls_[zone+"/"+bucket];
	if (!cur.empty())
	{
		CURL* res=cur.back();
		cur.pop_back();
		assert(!borrowed_curls_.count(res));
		borrowed_curls_[res]=zone+"/"+bucket;
		return curl_ptr_t(res, curl_deleter{this});
	} else
	{
		CURL* res=curl_easy_init();
		if (!res)
			err(errFatal) << "can't init CURL";

		char *err_buf=(char*)malloc(CURL_ERROR_SIZE+1);
		memset(err_buf, 0, CURL_ERROR_SIZE+1);

		error_bufs_[res]=err_buf;
		curl_easy_setopt(res, CURLOPT_ERRORBUFFER, err_buf);

		assert(!borrowed_curls_.count(res));
		borrowed_curls_[res]=zone+"/"+bucket;
		return curl_ptr_t(res, curl_deleter{this});
	}
}

void conn_context::release_curl(CURL* curl)
{
	guard_t lock(m_);

	assert(borrowed_curls_.count(curl));
	std::string key=borrowed_curls_.at(curl);
	borrowed_curls_.erase(curl);
	curls_[key].push_back(curl);
}
