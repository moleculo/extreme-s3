#include "connection.h"
#include <curl/curl.h>
#include "errors.h"
#include <openssl/hmac.h>
#include <tinyxml.h>
#include <iostream>
#include <assert.h>
#include "scope_guard.h"

using namespace es3;

void operator | (const CURLcode &code, const die_t &die)
{
	if (code!=CURLE_OK)
		err(result_code_t::sError) << "curl error: "
								   << curl_easy_strerror(code);
}

void check(const TiXmlDocument &doc)
{
	if (doc.Error())
		err(result_code_t::sError) << doc.ErrorDesc();
}

std::string escape(const std::string &str)
{
	char *res=curl_escape(str.c_str(), str.length());
	ON_BLOCK_EXIT(&curl_free,res);
	return std::string(res);
}

s3_connection::s3_connection(const connection_data &conn_data,
							 const std::string &verb,
							 const std::string &path,
							 const header_map_t &opts)
	: curl_(curl_easy_init()), conn_data_(conn_data),
	  opts_(opts), header_list_(), path_(path)
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

	curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1);
}

void s3_connection::set_url(const std::string &args)
{
	std::string url = conn_data_.use_ssl_?"https://" : "http://";
	url.append(conn_data_.bucket_).append(".s3.amazonaws.com");
	url.append(path_);
	url.append(args);
	curl_easy_setopt(curl_, CURLOPT_URL, url.c_str()) | die;
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
	size_t idx = path.find_first_of("?&");
	std::string canonic = path;
	if (idx!=std::string::npos)
		canonic = canonic.substr(0, idx);
	std::string canonicalizedResource="/"+conn_data_.bucket_+canonic;
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

file_map_t s3_connection::list_files(const std::string &prefix)
{
	file_map_t res;

	std::string marker;
	while(true)
	{
		if (prefix.empty())
			set_url("?marker="+escape(marker));
		else
			set_url("?prefix="+escape(prefix)+"&marker="+escape(marker));

		std::string list=read_fully();
		TiXmlDocument doc;
		doc.Parse(list.c_str()); check(doc);
		TiXmlHandle docHandle(&doc);

		TiXmlNode *node=docHandle.FirstChild("ListBucketResult")
				.FirstChild("Contents")
				.ToNode();
		if (!node)
			break;

		while(node)
		{
			std::string name = node->FirstChild("Key")->
					FirstChild()->ToText()->Value();
			std::string etag = node->FirstChild("ETag")->
					FirstChild()->ToText()->Value();
			std::string size = node->FirstChild("Size")->
					FirstChild()->ToText()->Value();
			if (size!="0")
			{
				deconstruct_file(res, name, etag, size);
			}

			node=node->NextSibling("Contents");
			if (!node)
				marker = name;
		}

		std::string is_trunc=docHandle.FirstChild("ListBucketResult")
				.FirstChild("IsTruncated").FirstChild().Text()->Value();
		if (is_trunc=="false")
			break;
	}

	return res;
}

void s3_connection::deconstruct_file(file_map_t &res,
									 const std::string &name,
									 const std::string &etag,
									 const std::string &size)
{
	assert(size!="0");
	std::string cur_name = name;

	file_map_t *cur_pos=&res;
	remote_file_weak_t cur_parent;
	while(true)
	{
		size_t pos=cur_name.find('/');
		if (pos==std::string::npos)
			break;
		if (pos==cur_name.size()-1)
			throw std::bad_exception();
		std::string component = cur_name.substr(0, pos);

		remote_file_ptr ptr;
		auto iter=cur_pos->find(component);
		if (iter!=cur_pos->end())
			ptr = iter->second;
		else
		{
			ptr = remote_file_ptr(new remote_file());
			ptr->is_dir_ = true;
			ptr->name_ = component;
			ptr->parent_ = cur_parent;
			(*cur_pos)[component] = ptr;
		}
		if (!ptr->is_dir_)
			throw std::bad_exception();
		cur_pos = &ptr->children_;
		cur_parent = ptr;

		cur_name = cur_name.substr(pos+1);
	}

	remote_file_ptr fl(new remote_file());
	fl->is_dir_ = false;
	fl->name_ = cur_name;
	fl->etag_ = etag;
	fl->size_ = atoll(size.c_str());
	fl->parent_ = cur_parent;
	if (cur_pos->find(cur_name)!=cur_pos->end())
		throw std::bad_exception();
	(*cur_pos)[cur_name] = fl;
}
