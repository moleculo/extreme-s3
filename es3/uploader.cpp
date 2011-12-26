#include "uploader.h"
#include <set>
#include <boost/filesystem.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <iostream>
#include <openssl/md5.h>
#include <scope_guard.h>

#define COMPRESSION_THRESHOLD 100000
#define MIN_RATIO 0.9d

#define MIN_PART_SIZE 10000000
#define MAX_PART_NUM 10

using namespace es3;
using namespace boost::interprocess;
using namespace boost::filesystem;

static file_mapping try_compress_and_open(const path &p, bool &compressed)
{
	compressed = false;

	uint64_t file_sz=file_size(p);
	if (file_sz <= COMPRESSION_THRESHOLD)
		return file_mapping(p.string().c_str(), read_only);

	//Check for GZIP magic
	char magic[4]={0};
	FILE *fl=fopen(p.string().c_str(), "rb");
	fread(magic, 4, 1, fl);
	fclose(fl);
	if (magic[0]==0x1F && magic[1] == 0x8B && magic[2]==0x8 && magic[3]==0x8)
		return file_mapping(p.string().c_str(), read_only);

	//Ok, try to compress the file
	path tmp_file = temp_directory_path() / unique_path();
	create_symlink(system_complete(p), tmp_file); //Create temp symlink
	ON_BLOCK_EXIT(unlink, tmp_file.c_str()); //Always delete the temp file

	int ret=system(("pigz -n -T -f "+tmp_file.string()).c_str());
	if (WEXITSTATUS(ret)!=0) //Not found or other problems
	{
		int ret2=system(("gzip -n -T -f "+tmp_file.string()).c_str());
		if (WEXITSTATUS(ret2)!=0)
			err(errFatal) << "Can't execute gzip/pigz on "<<p;
	}

	path gzipped = path(tmp_file.string()+".gz");
	ON_BLOCK_EXIT(unlink, gzipped.c_str()); //Always delete the temp file

	if (file_size(gzipped) >= file_sz*MIN_RATIO)
		return file_mapping(p.string().c_str(), read_only);

	compressed = true;
	return file_mapping(gzipped.string().c_str(), read_only);
}

struct es3::upload_content
{
	upload_content() : num_parts_(), num_completed_() {}

	std::recursive_mutex lock_;
	size_t num_parts_;
	size_t num_completed_;

	file_mapping mapping_;
	mapped_region region_;
};

void file_uploader::operator()(agenda_ptr agenda)
{
	//Create a file mapping.
	bool compressed = false;
	upload_content_ptr up_data(new upload_content());

	up_data->mapping_ = std::move(try_compress_and_open(path_, compressed));

	//Check for empty files
	offset_t sz;
	boost::interprocess::detail::get_file_size(
				up_data->mapping_.get_mapping_handle().handle, sz);
	if (sz==0)
	{
//		start_upload("", "", 0, false);
		//We do not upload empty files.
		//TODO: ??
		return;
	}
	//Map the whole file in this process
	up_data->region_ = std::move(mapped_region(up_data->mapping_,read_only));

	//Compute MD5
	unsigned char res[MD5_DIGEST_LENGTH+1]={0};
	MD5((const unsigned char*)up_data->region_.get_address(),
		up_data->region_.get_size(), res);
	std::string b64_md5=base64_encode((const char*)res, MD5_DIGEST_LENGTH);

	VLOG(2) << "Checking upload of " << path_ << " as "
			  << remote_;
	if (etag_.empty())
	{
		start_upload(agenda, b64_md5, up_data, compressed);
	} else
	{
		static const char alphabet[17]="0123456789abcdef";
		std::string md5_str;
		md5_str.resize(MD5_DIGEST_LENGTH*2);
		for(int f=0;f<MD5_DIGEST_LENGTH;++f)
		{
			md5_str[f*2]=alphabet[res[f]/16];
			md5_str[f*2+1]=alphabet[res[f]%16];
		}
		std::string md5_web;
		md5_web.push_back('\"');
		md5_web.append(md5_str);
		md5_web.push_back('\"');
		if (md5_web == etag_)
			return;

		start_upload(agenda, b64_md5, up_data, compressed);
	}
}

class part_upload_task : public sync_task
{
	size_t num_;
	const connection_data conn_;
	const std::string remote_;
	upload_content_ptr content_;
public:
	part_upload_task(size_t num, const connection_data &conn,
				  const std::string &remote, upload_content_ptr content)
		: num_(num), conn_(conn), remote_(remote), content_(content)
	{
	}

	virtual void operator()(agenda_ptr agenda)
	{
		VLOG(2) << "Starting upload of a part " << num_ << " of "
				<< remote_;
	}

};

void file_uploader::start_upload(agenda_ptr ag, const std::string &md5,
								 upload_content_ptr content, bool compressed)
{
	VLOG(2) << "Starting upload of " << path_ << " as "
			<< remote_ << ", MD5: "<< md5;

	size_t size=content->region_.get_size();
	if (size>MIN_PART_SIZE)
	{
		size_t number_of_parts = (size+MIN_PART_SIZE/2)/MIN_PART_SIZE;
		if (number_of_parts>MAX_PART_NUM)
			number_of_parts = MAX_PART_NUM;

		header_map_t hmap;
		hmap["x-amz-meta-compressed"] = compressed ? "true" : "false";
		hmap["x-amz-meta-md5"] = md5;
		hmap["Content-Type"] = "application/x-binary";
		s3_connection up(conn_, "POST", remote_, hmap);
		std::string upload_id=up.initiate_multipart();

		for(size_t f=0;f<number_of_parts;++f)
		{
			boost::shared_ptr<part_upload_task> task(
						new part_upload_task(f, conn_, remote_, content));
			ag->schedule(task);
		}
	} else
	{
		header_map_t hmap;
		hmap["x-amz-meta-compressed"] = compressed ? "true" : "false";
		hmap["Content-MD5"] = md5;
		hmap["Content-Type"] = "application/x-binary";
		s3_connection up(conn_, "PUT", remote_, hmap);
		up.upload_data(content->region_.get_address(),
					   content->region_.get_size());
	}
}


void file_deleter::operator()(agenda_ptr agenda)
{
	VLOG(2) << "Removing " << remote_;
	header_map_t hmap;
	s3_connection up(conn_, "DELETE", remote_, hmap);
	up.read_fully();
}
