#include "connection.h"
#include "context.h"
#include <curl/curl.h>
#include "errors.h"
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <tinyxml.h>
#include "scope_guard.h"
#include <boost/algorithm/string.hpp>

using namespace es3;

static std::string escape(const std::string &str)
{
	char *res=curl_escape(str.c_str(), str.length());
	ON_BLOCK_EXIT(&curl_free,res);
	return std::string(res);
}

s3_path es3::parse_path(const std::string &url)
{
	s3_path res;
	if (url.find("s3://")!=0)
		throw std::bad_exception();

	std::string bucket_and_path=url.substr(strlen("s3://"));
	size_t path_pos = bucket_and_path.find('/');
	if (path_pos==0)
		err(errFatal) << "Malformed S3 URL - no bucket name: " << url;

	if (path_pos!=-1)
	{
		res.bucket_=bucket_and_path.substr(0, path_pos);
		res.path_=bucket_and_path.substr(path_pos);
	} else
	{
		res.bucket_=bucket_and_path;
		res.path_="/";
	}

	return res;
}

s3_connection::s3_connection(const context_ptr &conn_data)
	: curl_(curl_easy_init()), conn_data_(conn_data), header_list_()
{
	if (!curl_)
		err(errFatal) << "can't init CURL";
	curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, error_buffer_);
	memset(error_buffer_, 0, CURL_ERROR_SIZE);
}

void s3_connection::checked(int curl_code)
{
	if (curl_code!=CURLE_OK)
	{
		if (strlen(error_buffer_)!=0)
		{
			assert(strlen(error_buffer_)<=CURL_ERROR_SIZE);
			err(errWarn) << "curl error: " << error_buffer_;
		} else
			err(errWarn) << "curl error: "
						 << curl_easy_strerror((CURLcode)curl_code);
	}
}

void s3_connection::check_for_errors(const std::string &curl_res)
{
	long code=400;
	checked(curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &code));
	if (code<400)
		return;

	code_e err_level=errFatal;
	if (code>=500)
		err_level=errWarn;
	std::string def_error="HTTP code "+int_to_string(code)+" received.";

	TiXmlDocument doc;
	doc.Parse(curl_res.c_str());
	if (!doc.Error())
	{
		TiXmlHandle docHandle(&doc);
		TiXmlNode *s3_err_code=docHandle.FirstChild("Error")
				.FirstChild("Code")
				.FirstChild()
				.ToText();
		TiXmlNode *message=docHandle.FirstChild("Error")
				.FirstChild("Message")
				.FirstChild()
				.ToText();
		if (s3_err_code && message)
			err(err_level) << s3_err_code->Value()
						   << " - " << message->Value();
	} else
		err(err_level) << "" << def_error;
}

void s3_connection::prepare(const std::string &verb,
		  const s3_path &path,
		  const header_map_t &opts)
{
	s3_path cur_path=path;
	if (cur_path.path_.empty())
		cur_path.path_.append("/");

	curl_easy_reset(curl_);
	curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, error_buffer_);
	memset(error_buffer_, 0, CURL_ERROR_SIZE);

	//Set HTTP verb
	checked(curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, verb.c_str()));
	//Do not do any automatic decompression
	checked(curl_easy_setopt(curl_, CURLOPT_ENCODING , 0));

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
	checked(curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list_));
	checked(curl_easy_setopt(curl_, CURLOPT_BUFFERSIZE, 16384*16));

	checked(curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1));
	set_url(path, "");
}

void s3_connection::set_url(const s3_path &path, const std::string &args)
{
	s3_path cur_path=path;
	if (cur_path.path_.empty())
		cur_path.path_.append("/");

	std::string url = conn_data_->use_ssl_?"https://" : "http://";
	url.append(cur_path.bucket_);
	url.append(".").append(cur_path.zone_);
	url.append(".amazonaws.com");
//	std::string ugly=cur_path.path_;
//	boost::replace_all(ugly, " ", "%20");
//	url.append(ugly);
	url.append(cur_path.path_);
	url.append(args);
	checked(curl_easy_setopt(curl_, CURLOPT_URL, url.c_str()));
}

curl_slist* s3_connection::authenticate_req(struct curl_slist * header_list,
	const std::string &verb, const s3_path &path, const header_map_t &opts)
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
	std::string canonicalizedResource="/"+path.bucket_+path.path_;
	std::string stringToSign = verb + "\n" +
		try_get(opts, "content-md5") + "\n" +
		try_get(opts, "content-type") + "\n" +
		/*date_header +*/ "\n" +
		amz_headers +
		canonicalizedResource;
	std::string sign_res=sign(stringToSign) ;
	if (sign_res.empty())
		sign_res.empty();

	std::string auth="Authorization: AWS "+conn_data_->api_key_+":"+sign_res;
	return curl_slist_append(header_list, auth.c_str());
}

std::string s3_connection::sign(const std::string &str)
{
	if (str.length() >= INT_MAX)
		throw std::bad_exception();

	const std::string &secret_key = conn_data_->secret_key;
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
									  const s3_path &path,
									  const std::string &args,
									  const header_map_t &opts)
{
	std::string res;
	prepare(verb, path, opts);
	if (!args.empty())
		set_url(path, args);
	checked(curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender));
	checked(curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &res));
	checked(curl_easy_perform(curl_));
	check_for_errors(res);
	return res;
}

s3_directory_ptr s3_connection::list_files(const s3_path &path,
										   bool try_to_root)
{
	s3_directory_ptr res(new s3_directory());
	res->absolute_name_ = path;

	if (try_to_root && *path.path_.rbegin() != '/')
	{
		size_t pos=path.path_.find_last_of('/');
		if (pos==std::string::npos)
			res->absolute_name_.path_ = "/";
		else
			res->absolute_name_.path_ = path.path_.substr(0, pos+1);
	} else
	{
	}

	std::string marker;
	while(true)
	{
		std::string args;
		assert(!path.path_.empty() && path.path_[0]=='/');

		std::string no_slash = path.path_.substr(1);
		if (no_slash.empty())
			args="?marker="+escape(marker);
		else
			args="?prefix="+escape(no_slash)+"&marker="+escape(marker);

		s3_path root=path;
		root.path_="/";
		std::string list=read_fully("GET", root, args);

		TiXmlDocument doc;
		doc.Parse(list.c_str());
		if (doc.Error())
			err(errWarn) << "Failed to get file listing from /" << path;
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
			std::string size = node->FirstChild("Size")->
					FirstChild()->ToText()->Value();
			//if (size!="0")
			{
				deconstruct_file(res, name, size);
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

void s3_connection::deconstruct_file(s3_directory_ptr res,
									 const std::string &name,
									 const std::string &size)
{
//	assert(size!="0");
	std::string cur_name = "/"+name;

	//First, snip off the common path
	assert(cur_name.find(res->absolute_name_.path_)==0);
	cur_name = cur_name.substr(res->absolute_name_.path_.size());

	s3_directory_ptr cur_dir=res;
	//While advancing the cur_dir, snip off the directory components
	//from cur_name until we reach the leaf (i.e. the filename)
	while(true)
	{
		size_t pos=cur_name.find('/');
		if (pos==std::string::npos)
			break;
		if (pos==cur_name.size()-1)
			throw std::bad_exception();
		std::string component = cur_name.substr(0, pos);

		s3_directory_ptr dir;
		auto iter=cur_dir->subdirs_.find(component);
		if (iter!=cur_dir->subdirs_.end())
			dir = iter->second; //There's a subdir with this name already
		else
		{
			dir = s3_directory_ptr(new s3_directory());
			dir->absolute_name_ = derive(cur_dir->absolute_name_, component);
			dir->name_ = component;
			dir->parent_ = cur_dir;
			cur_dir->subdirs_[component] = dir;
		}

		cur_dir = dir;
		cur_name = cur_name.substr(pos+1);
	}

	s3_file_ptr fl(new s3_file());
	fl->name_ = cur_name;
	fl->absolute_name_ = derive(cur_dir->absolute_name_, cur_name);
	fl->size_ = atoll(size.c_str());
	fl->parent_ = cur_dir;

	if (cur_dir->files_.find(cur_name)!=cur_dir->files_.end())
		throw std::bad_exception();
	cur_dir->files_[cur_name] = fl;
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
	file_desc *info=reinterpret_cast<file_desc*>(userdata);

	std::string mtime=find_header(ptr, size, nmemb,
								  "x-amz-meta-last-modified");
	if (!mtime.empty())
		info->mtime_=atoll(mtime.c_str());

	std::string ln=find_header(ptr, size, nmemb, "x-amz-meta-size");
	if (!ln.empty())
		info->raw_size_ = atoll(ln.c_str());

	std::string ln2=find_header(ptr, size, nmemb, "content-length");
	if (!ln2.empty())
		info->remote_size_ = atoll(ln2.c_str());

	std::string cmpr=find_header(ptr, size, nmemb, "x-amz-meta-compressed");
	if (cmpr=="true")
		info->compressed=true;

	std::string md=find_header(ptr, size, nmemb, "x-amz-meta-file-mode");
	if (!md.empty())
		info->mode_ = atoll(md.c_str());

	return size*nmemb;
}

static size_t find_etag(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	std::string etag=find_header(ptr, size, nmemb, "etag");
	if (!etag.empty())
		reinterpret_cast<std::string*>(userdata)->assign(etag);
	return size*nmemb;
}

file_desc s3_connection::find_mtime_and_size(const s3_path &path)
{
	file_desc result;
	result.compressed=false;
	result.mode_ = 0664;
	result.remote_size_=result.raw_size_=0;

	prepare("HEAD", path);
	//last-modified
	checked(curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &::find_mtime));
	checked(curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &result));
	checked(curl_easy_setopt(curl_, CURLOPT_NOBODY, 1));
	checked(curl_easy_perform(curl_));

	if (result.raw_size_==0)
		result.raw_size_=result.remote_size_;
	return result;
}

class buf_data
{
	const char *buf_;
	size_t total_size_;
	size_t written_;
	MD5_CTX md5_ctx;
public:
	buf_data(const char *buf, size_t total_size)
		: buf_(buf), total_size_(total_size), written_()
	{
		MD5_Init(&md5_ctx);
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
		return reinterpret_cast<buf_data*>(userp)->simple_read(
					bufptr, size*nitems);
	}

	size_t simple_read(char *bufptr, size_t size)
	{
		size_t tocopy = std::min(total_size_-written_, size);
		if (tocopy!=0)
		{
			memcpy(bufptr, buf_+written_, tocopy);
			MD5_Update(&md5_ctx, bufptr, tocopy);
			written_+=tocopy;
		}
		return tocopy;
	}
};

std::string s3_connection::upload_data(const s3_path &path,
	const char *data, size_t size, const header_map_t& opts)
{
	assert(data);

	std::string etag;
	buf_data read_data(data, size);

	prepare("PUT", path, opts);
	checked(curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &find_etag));
	checked(curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &etag));
	checked(curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1));
	checked(curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE,
							 uint64_t(size)));
	checked(curl_easy_setopt(curl_, CURLOPT_READFUNCTION,
							 &buf_data::read_func));
	checked(curl_easy_setopt(curl_, CURLOPT_READDATA, &read_data));

	std::string result;
	checked(curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender));
	checked(curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &result));

	checked(curl_easy_perform(curl_));
	check_for_errors(result);

	if (!etag.empty() &&
			strcasecmp(etag.c_str(), ("\""+read_data.get_md5()+"\"").c_str()))
		abort(); //Data corruption. This SHOULD NOT happen!

	return etag;
}

std::string s3_connection::initiate_multipart(
	const s3_path &path, const header_map_t &opts)
{
	set_url(path, "");
	s3_path up_path =path;
	up_path.path_+="?uploads";
	std::string list=read_fully("POST", up_path, "", opts);

	TiXmlDocument doc;
	doc.Parse(list.c_str());
	if (doc.Error())
		err(errWarn) << "Failed to initiate multipart to " << path;
	TiXmlHandle docHandle(&doc);

	TiXmlNode *node=docHandle.FirstChild("InitiateMultipartUploadResult")
			.FirstChild("UploadId")
			.FirstChild()
			.ToText();
	if (!node)
		err(errWarn) << "Incorrect document format - no upload ID";
	return node->Value();
}

std::string s3_connection::complete_multipart(const s3_path &path,
	const std::string &upload_id, const std::vector<std::string> &etags)
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

	s3_path up_path=path;
	up_path.path_+="?uploadId="+upload_id;
	prepare("POST", up_path);

	buf_data data_params(data.c_str(), data.size());
	checked(curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1));
	checked(curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE,
							 uint64_t(data.size())));
	checked(curl_easy_setopt(curl_, CURLOPT_READFUNCTION,
							 &buf_data::read_func));
	checked(curl_easy_setopt(curl_, CURLOPT_READDATA, &data_params));

	std::string read_data;
	checked(curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &string_appender));
	checked(curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &read_data));

	checked(curl_easy_perform(curl_));
	check_for_errors(read_data);

	VLOG(2) << "Completed multipart of " << path;
	return read_data;
}

class write_data
{
	char *buf_;
	size_t total_size_;
	size_t written_;
public:
	write_data(char *buf, size_t total_size)
		: buf_(buf), total_size_(total_size), written_()
	{
	}

	size_t written() const { return written_; }

	static size_t write_func(const char *bufptr, size_t size,
							size_t nitems, void *userp)
	{
		return reinterpret_cast<write_data*>(userp)->simple_write(
					bufptr, size*nitems);
	}

	size_t simple_write(const char *bufptr, size_t size)
	{
		size_t tocopy = std::min(total_size_-written_, size);
		if (tocopy!=0)
		{
			memcpy(buf_+written_, bufptr, tocopy);
			written_+=tocopy;
		}
		return tocopy;
	}
};

void s3_connection::download_data(const s3_path &path,
	uint64_t offset, char *data, size_t size, const header_map_t& opts)
{
	prepare("GET", path, opts);
	curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, uint64_t(size));

	std::string range=int_to_string(offset)+"-"+
			int_to_string(offset+size-1);
	curl_easy_setopt(curl_, CURLOPT_RANGE, range.c_str());

	write_data wd(data, size);
	checked(curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION,
							 &write_data::write_func));
	checked(curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &wd));

	checked(curl_easy_perform(curl_));
	check_for_errors(std::string(data,
								 std::min(wd.written(), size_t(1024))));

	if (wd.written()!=size)
		err(errWarn)  << "Size of a segment at offset " << offset
					  << " of "<< path << " is incorrect.";
}

std::string s3_connection::find_region(const std::string &bucket)
{
	s3_path path;
	path.zone_ = "s3";
	path.bucket_ = bucket;
	path.path_ = "/?location";

	std::string reg_data=read_fully("GET", path);
	check_for_errors(reg_data);

	TiXmlDocument doc;
	doc.Parse(reg_data.c_str());
	if (doc.Error())
		err(errWarn) << "Can't find region, bad document received. "
					 << doc.ErrorDesc();
	TiXmlHandle docHandle(&doc);

	TiXmlNode *n1=docHandle.FirstChild("LocationConstraint").ToNode();
	if (!n1)
		err(errWarn) << "Incorrect document format - no location id";

	TiXmlNode *node=docHandle.FirstChild("LocationConstraint")
			.FirstChild()
			.ToText();
	if (!node)
		return "s3"; //Default location
	return node->Value();
}
