#include "connection.h"
#include <curl/curl.h>
#include "errors.h"
#include <openssl/hmac.h>
#include <tinyxml.h>
#include <iostream>
#include <assert.h>
#include <strings.h>
#include "scope_guard.h"

using namespace es3;

bool es3::ci_string_less::operator()(const std::string &lhs,
									 const std::string &rhs) const
{
	return strcasecmp(lhs.c_str(), rhs.c_str()) < 0 ? 1 : 0;
}

void operator | (const CURLcode &code, const die_t &die)
{
	if (code!=CURLE_OK)
		err(errFatal) << "curl error: "
					  << curl_easy_strerror(code);
}

void check(const TiXmlDocument &doc)
{
	if (doc.Error())
		err(errFatal) << doc.ErrorDesc();
}

std::string escape(const std::string &str)
{
	char *res=curl_escape(str.c_str(), str.length());
	ON_BLOCK_EXIT(&curl_free,res);
	return std::string(res);
}

s3_connection::s3_connection(const connection_data &conn_data,
							 const std::string &verb,
							 const std::string &path_raw,
							 const header_map_t &opts)
	: curl_(curl_easy_init()), conn_data_(conn_data),
	  opts_(opts), header_list_(), path_(path_raw)
{
	if (!curl_)
		err(errFatal) << "can't init CURL";
	if (path_.empty())
		path_.append("/");

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

	header_list_ = authenticate_req(header_list_, verb, path_);
	curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list_) | die;

	curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1);
	set_url("");
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

	std::string amz_headers;
	for(auto iter = opts_.begin(); iter!=opts_.end();++iter)
	{
		std::string lower_hdr = iter->first;
		std::transform(lower_hdr.begin(), lower_hdr.end(),
					   lower_hdr.begin(), ::tolower);
		if (lower_hdr.find("x-amz-meta")==0)
			amz_headers.append(lower_hdr).append(":")
					.append(iter->second).append("\n");
	}

	//Signature
//	size_t idx = path.find_first_of("?&");
	std::string canonic = path;
//	if (idx!=std::string::npos)
//		canonic = canonic.substr(0, idx);
	std::string canonicalizedResource="/"+conn_data_.bucket_+canonic;
	std::string stringToSign = verb + "\n" +
		try_get(opts_, "content-md5") + "\n" +
		try_get(opts_, "content-type") + "\n" +
		date_header + "\n" +
		amz_headers +
		canonicalizedResource;
	std::string sign_res=sign(stringToSign) ;
	if (sign_res.empty())
		sign_res.empty();

	std::string auth="Authorization: AWS "+conn_data_.api_key_+":"+sign_res;
	return curl_slist_append(header_list, auth.c_str());
}

std::string s3_connection::sign(const std::string &str)
{
	if (str.length() >= INT_MAX)
		throw std::bad_exception();

	const std::string &secret_key = conn_data_.secret_key;
	char md[EVP_MAX_MD_SIZE+1]={0};
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
	//TODO: check error code
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

	return std::move(res);
}

void s3_connection::deconstruct_file(file_map_t &res,
									 const std::string &name,
									 const std::string &etag,
									 const std::string &size)
{
	assert(size!="0");
	std::string cur_name = name;

	file_map_t *cur_pos=&res;
	remote_file_ptr cur_parent;
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
			ptr->full_name_ = cur_parent?
						(cur_parent->full_name_+"/"+component) :
						(conn_data_.remote_root_+component);
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
	fl->full_name_ = conn_data_.remote_root_+name;
	fl->etag_ = etag;
	fl->size_ = atoll(size.c_str());
	fl->parent_ = cur_parent;
	if (cur_pos->find(cur_name)!=cur_pos->end())
		throw std::bad_exception();
	(*cur_pos)[cur_name] = fl;
}

struct read_data
{
	const char *buf;
	size_t off_, total_;
};

size_t read_func(char *bufptr, size_t size, size_t nitems, void *userp)
{
	read_data *data = (read_data*) userp;
	size_t tocopy = std::min(data->total_-data->off_, size*nitems);
	if (tocopy!=0)
	{
		memcpy(bufptr, data->buf+data->off_, tocopy);
		data->off_+=tocopy;
	}
	return tocopy;
}

static size_t find_header(void *ptr, size_t size, size_t nmemb, void *userdata,
				   const std::string &header_name)
{
	std::string line(reinterpret_cast<char*>(ptr), size*nmemb);
	std::string line_raw(reinterpret_cast<char*>(ptr), size*nmemb);
	std::transform(line.begin(), line.end(), line.begin(), ::tolower);

	size_t pos=line.find(':');
	if(pos!=std::string::npos)
	{
		std::string name=trim(line.substr(0, pos));
		std::string val=trim(line_raw.substr(pos+1));
		if (name==header_name)
			reinterpret_cast<std::string*>(userdata)->assign(val);
	}

	return size*nmemb;
}

static size_t find_md5(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return find_header(ptr, size, nmemb, userdata, "x-amz-meta-md5");
}

static size_t find_etag(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return find_header(ptr, size, nmemb, userdata, "etag");
}

std::string s3_connection::find_md5()
{
	set_url("");
	std::string result;
	curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &::find_md5);
	curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &result);
	curl_easy_setopt(curl_, CURLOPT_NOBODY, 1);
	curl_easy_perform(curl_) | die;
	return result;
}

std::string s3_connection::upload_data(const void *addr, size_t size,
									   size_t part_num,
									   const std::string &uploadId)
{
	std::string result;

	//Set data
	if (uploadId.empty())
		set_url("");
	else
	{
		set_url("");
		curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &find_etag);
		curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &result);
	}

	read_data data = {(const char*)addr, 0, size};
//	curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, uint64_t(size));
	curl_easy_setopt(curl_, CURLOPT_READFUNCTION, &read_func);
	curl_easy_setopt(curl_, CURLOPT_READDATA, &data);

	std::string read_data;
	curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender);
	curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &read_data);

	curl_easy_perform(curl_);

	return result;
}

std::string s3_connection::initiate_multipart()
{
	set_url("");
	std::string list=read_fully();
	TiXmlDocument doc;
	doc.Parse(list.c_str()); check(doc);
	TiXmlHandle docHandle(&doc);

	TiXmlNode *node=docHandle.FirstChild("InitiateMultipartUploadResult")
			.FirstChild("UploadId")
			.FirstChild()
			.ToText();
	if (!node)
		err(errFatal) << "Incorrect document format - no upload ID";
	return node->Value();
}

std::string s3_connection::complete_multipart(
	const std::vector<std::string> &etags)
{
	std::string data="<CompleteMultipartUpload>";
	for(size_t f=0;f<etags.size();++f)
	{
		data.append("<Part>\n");
		data.append("  <PartNumber>")
				.append(int_to_string(f+1))
				.append("</PartNumber>\n");
		data.append("  <ETag>")
				.append(etags.at(f))
				.append("</ETag>\n");
		data.append("</Part>\n");
	}
	data.append("</CompleteMultipartUpload>");

	set_url("");

	read_data data_params = {(const char*)&data[0], 0, data.size()};
	curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, uint64_t(data.size()));
	curl_easy_setopt(curl_, CURLOPT_READFUNCTION, &read_func);
	curl_easy_setopt(curl_, CURLOPT_READDATA, &data_params);

	std::string read_data;
	curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender);
	curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &read_data);

	curl_easy_perform(curl_);

	VLOG(2) << read_data;
	return read_data;
}
