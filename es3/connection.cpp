#include "connection.h"
#include <curl/curl.h>
#include "errors.h"
#include <openssl/hmac.h>
#include <openssl/md5.h>
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

s3_connection::s3_connection(const connection_data &conn_data)
	: curl_(curl_easy_init()), conn_data_(conn_data), header_list_()
{
	if (!curl_)
		err(errFatal) << "can't init CURL";
}

void s3_connection::prepare(const std::string &verb,
		  const std::string &path,
		  const header_map_t &opts)
{
	std::string cur_path=path;
	if (cur_path.empty())
		cur_path.append("/");

	//Set HTTP verb
	curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, verb.c_str()) | die;

	if (header_list_)
	{
		curl_slist_free_all(header_list_);
		header_list_ = 0;
	}

	//Add custom headers
	for(auto iter = opts.begin(), iend=opts.end(); iter!=iend; ++iter)
	{
		std::string header = iter->first + ": " + iter->second;
		header_list_ = curl_slist_append(header_list_, header.c_str());
	}

	header_list_ = authenticate_req(header_list_, verb, cur_path, opts);
	curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list_) | die;
	curl_easy_setopt(curl_, CURLOPT_BUFFERSIZE, 65536*4);
	curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 5);

	curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1);
	set_url(cur_path, "");
}

void s3_connection::set_url(const std::string &path, const std::string &args)
{
	std::string cur_path=path;
	if (cur_path.empty())
		cur_path.append("/");

	std::string url = conn_data_.use_ssl_?"https://" : "http://";
	url.append(conn_data_.bucket_).append(".s3.amazonaws.com");
	url.append(cur_path);
	url.append(args);
	curl_easy_setopt(curl_, CURLOPT_URL, url.c_str()) | die;
}

curl_slist* s3_connection::authenticate_req(struct curl_slist * header_list,
	const std::string &verb, const std::string &path, const header_map_t &opts)
{
	header_map_t my_opts = opts;

	//Make a 'Date' header
	std::string date_header=format_time(time(NULL));
	my_opts["x-amz-date"] = date_header;
	header_list = curl_slist_append(header_list,
		(std::string("x-amz-date: ")+date_header).c_str());

	std::string amz_headers;
	for(auto iter = my_opts.begin(); iter!=my_opts.end();++iter)
	{
		std::string lower_hdr = iter->first;
		std::transform(lower_hdr.begin(), lower_hdr.end(),
					   lower_hdr.begin(), ::tolower);
		if (lower_hdr.find("x-amz")==0)
			amz_headers.append(lower_hdr).append(":")
					.append(iter->second).append("\n");
	}

	//Signature
	std::string canonicalizedResource="/"+conn_data_.bucket_+path;
	std::string stringToSign = verb + "\n" +
		try_get(opts, "content-md5") + "\n" +
		try_get(opts, "content-type") + "\n" +
		/*date_header +*/ "\n" +
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

std::string s3_connection::read_fully(const std::string &verb,
									  const std::string &path,
									  const header_map_t &opts)
{
	std::string res;
	prepare(verb, path, opts);
	curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender);
	curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &res);
	curl_easy_perform(curl_) | die;
	//TODO: check error code
	return res;
}

file_map_t s3_connection::list_files(const std::string &path,
	const std::string &prefix)
{
	file_map_t res;
	std::string marker;
	while(true)
	{
		std::string cur_path;
		if (prefix.empty())
			cur_path=path+"?marker="+escape(marker);
		else
			cur_path=path+"?prefix="+escape(prefix)+"&marker="+escape(marker);

		std::string list;
		prepare("GET", path);
		set_url(cur_path, "");
		curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender);
		curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &list);
		curl_easy_perform(curl_) | die;

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

static std::string find_header(void *ptr, size_t size, size_t nmemb,
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
			return val;
	}

	return "";
}

static size_t find_mtime(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	std::pair<time_t, uint64_t> *pair=
			reinterpret_cast<std::pair<time_t,uint64_t>*>(userdata);

	std::string mtime=find_header(ptr, size, nmemb,
								  "x-amz-meta-last-modified");
	if (!mtime.empty())
	{
		int64_t tm2 = atoll(mtime.c_str());
		if (tm2 < pair->first)
			pair->first=tm2;
	}

	std::string ln=find_header(ptr, size, nmemb, "x-amz-meta-size");
	if (!ln.empty())
		pair->second = atoll(ln.c_str());

	return size*nmemb;
}

static size_t find_etag(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	std::string etag=find_header(ptr, size, nmemb, "etag");
	if (!etag.empty())
		reinterpret_cast<std::string*>(userdata)->assign(etag);
	return size*nmemb;
}

std::pair<time_t, uint64_t> s3_connection::find_mtime_and_size(
	const std::string &path)
{
	std::pair<time_t, uint64_t> result={time(NULL), 0};
	prepare("HEAD", path);
	//last-modified
	curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &::find_mtime);
	curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &result);
	curl_easy_setopt(curl_, CURLOPT_NOBODY, 1);
	curl_easy_perform(curl_) | die;
	return result;
}

class read_data
{
	handle_t descriptor_;
	uint64_t offset_, size_;

	uint64_t written_;
	MD5_CTX md5_ctx;
public:
	read_data(const handle_t &descriptor, size_t size, size_t offset) :
		descriptor_(descriptor), offset_(offset), size_(size), written_()
	{
		MD5_Init(&md5_ctx);
		uint64_t res=lseek64(descriptor_.get(), offset_, SEEK_SET);
		if (res<0)
			res | libc_die;
	}

	std::string get_md5()
	{
		unsigned char md[MD5_DIGEST_LENGTH+1]={0};
		MD5_Final(md, &md5_ctx);
		return tobinhex(md, MD5_DIGEST_LENGTH);
	}

	static size_t read_func(char *bufptr, size_t size,
							size_t nitems, void *userp)
	{
		return reinterpret_cast<read_data*>(userp)->do_read(
					bufptr, size*nitems);
	}

	size_t do_read(char *bufptr, size_t size)
	{
		size_t tocopy = std::min(size_-written_, uint64_t(size));
		size_t res=read(descriptor_.get(), bufptr, tocopy);
		if (res>0)
		{
			MD5_Update(&md5_ctx, bufptr, res);
			written_+=res;
		}
		return res;
	}
};

std::string s3_connection::upload_data(const std::string &path,
	const handle_t &descriptor, uint64_t size, uint64_t offset,
	const header_map_t& opts)
{
	prepare("PUT", path, opts);
	if (size==0)
	{
		lseek64(descriptor.get(), 0, SEEK_SET);
		size=lseek64(descriptor.get(), 0, SEEK_END);
	}

	std::string etag;
	curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &find_etag);
	curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &etag);

	read_data data(descriptor, size, offset);
	curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, size);
	curl_easy_setopt(curl_, CURLOPT_READFUNCTION, &read_data::read_func);
	curl_easy_setopt(curl_, CURLOPT_READDATA, &data);

	std::string result;
	curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender);
	curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &result);

	curl_easy_perform(curl_) | die;
	if (result.find("<Error>")!=std::string::npos)
		err(errWarn) << "Upload failed: "<<result;
	std::cerr << "Upload data " << result << std::endl;

	if (!etag.empty() &&
			strcasecmp(etag.c_str(), ("\""+data.get_md5()+"\"").c_str()))
		abort(); //Data corruption. This SHOULD NOT happen!

	return etag;
}

std::string s3_connection::initiate_multipart(
	const std::string &path, const header_map_t &opts)
{
	set_url(path, "");
	std::string list=read_fully("POST", path+"?uploads", opts);
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

struct buf_data
{
	const char *buf_;
	size_t raw_size_;
	size_t written_;

	static size_t read_func(char *bufptr, size_t size,
							size_t nitems, void *userp)
	{
		return reinterpret_cast<buf_data*>(userp)->simple_read(
					bufptr, size*nitems);
	}

	size_t simple_read(char *bufptr, size_t size)
	{
		size_t tocopy = std::min(raw_size_-written_, size);
		if (tocopy!=0)
		{
			memcpy(bufptr, buf_+written_, tocopy);
			written_+=tocopy;
		}
		return tocopy;
	}
};

std::string s3_connection::complete_multipart(const std::string &path,
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

	prepare("POST", path);

	buf_data data_params {data.c_str(), data.length(), 0};
	curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, uint64_t(data.size()));
	curl_easy_setopt(curl_, CURLOPT_READFUNCTION, &buf_data::read_func);
	curl_easy_setopt(curl_, CURLOPT_READDATA, &data_params);

	std::string read_data;
	curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender);
	curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &read_data);

	curl_easy_perform(curl_) | die;

	VLOG(2) << "Complete multipart " << read_data;
	return read_data;
}
