#include "connection.h"
#include <curl/curl.h>
#include "errors.h"
#include <openssl/hmac.h>
#include <iostream>

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
		if (strcasecmp(iter->first.c_str(), "date")==0)
			continue; //Silently skip the date header

		std::string header = iter->first + ": " + iter->second;
		header_list_ = curl_slist_append(header_list_, header.c_str());
	}

	header_list_ = authenticate_req(header_list_, verb, path);
	curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list_) | die;

	std::string url = conn_data.use_ssl_?"https://" : "http://";
	url.append(conn_data.bucket_).append(".s3.amazonaws.com");
	url.append(path);

	curl_easy_setopt(curl_, CURLOPT_URL, url.c_str()) | die;
	curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1);
}

curl_slist* s3_connection::authenticate_req(struct curl_slist * header_list,
	const std::string &verb, const std::string &path)
{
	//Make a 'Date' header
	time_t rawtime = time (NULL);
	struct tm * timeinfo = gmtime(&rawtime);
	char date_header[80] = {0};
	strftime(date_header, 80, "%a, %d %b %Y %H:%M:%S +0000", timeinfo);
	header_list = curl_slist_append(header_list,
									 (std::string("Date: ")+date_header).c_str());

	//Signature
	std::string canonicalizedResource="/"+conn_data_.bucket_+path;
	std::string stringToSign = verb + "\n" +
		try_get(opts_, "content-md5") + "\n" +
		try_get(opts_, "content-type") + "\n" +
		date_header + "\n" +
//		/*CanonicalizedAmzHeaders +*/ "\n" +
		canonicalizedResource;
	std::string sign_res=sign(stringToSign) ;

	std::string auth="Authorization: AWS "+conn_data_.api_key_+":"+sign_res;
	return curl_slist_append(header_list, auth.c_str());
}

std::string s3_connection::sign(const std::string &str)
{
	if (str.length() >= INT_MAX)
		throw std::bad_exception();

	const std::string &secret_key = conn_data_.secret_key;
	char md[EVP_MAX_MD_SIZE]={0};
	unsigned int md_len=0;
	HMAC(EVP_sha1(),
				  secret_key.c_str(),
				  secret_key.length(),
				  (const unsigned char*)str.c_str(), str.length(),
				  (unsigned char*)md, &md_len);
	return base64_encode(md, md_len);
}

s3_connection::~s3_connection()
{
	curl_easy_cleanup(curl_);
	if (header_list_)
		curl_slist_free_all(header_list_);
}

static size_t string_appender(const char *ptr,
							  size_t size, size_t nmemb, void *userdata)
{
	std::string *str = reinterpret_cast<std::string*>(userdata);
	str->append(ptr, ptr+size*nmemb);
	return size*nmemb;
}

std::string s3_connection::read_fully()
{
	std::string res;
	curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender);
	curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &res);
	curl_easy_perform(curl_) | die;
	return res;
}
