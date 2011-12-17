#include "connection.h"
#include <curl/curl.h>
#include "errors.h"

using namespace es3;

void operator | (const CURLcode &code, const die_t &die)
{
	if (code!=CURLE_OK)
		err(result_code_t::sError) << "curl error: "
								   << curl_easy_strerror(code);
}

s3_connection::s3_connection(const connection_data &conn_data,
							 const std::string &verb,
							 const std::string &path,
							 const header_map_t &opts)
	: curl_(curl_easy_init()), conn_data_(conn_data),
	  opts_(opts), header_list_()
{
	if (!curl_)
		err(result_code_t::sError) << "can't init CURL";

	//Set HTTP verb
	curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, verb.c_str()) | die;

	//Add custom headers
	for(auto iter = opts.begin(), iend=opts.end(); iter!=iend; ++iter)
	{
		std::string header = iter->first + ": " + iter->second;
		header_list_ = curl_slist_append(header_list_, header.c_str());
	}
	curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list_) | die;

	std::string url = conn_data.use_ssl_?"https://" : "http://";
	url.append(conn_data.bucket_).append(".s3.amazonaws.com/");
	url.append(conn_data.bucket_);
	if (url.at(url.length()-1)!='/') url.append("/");
	url.append(path);

	curl_easy_setopt(curl_, CURLOPT_URL, url.c_str()) | die;

	//s3.amazonaws.com
//	curl_set
}

s3_connection::~s3_connection()
{
	curl_easy_cleanup(curl_);
	if (header_list_)
		curl_slist_free_all(header_list_);
}

std::string s3_connection::read_fully()
{
	curl_easy_perform(curl_) | die;
	return "";
}
