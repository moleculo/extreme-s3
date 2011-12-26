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

//#define MIN_PART_SIZE 10000000
#define MIN_PART_SIZE 5242880
#define MAX_PART_NUM 10

using namespace es3;
using namespace boost::interprocess;
using namespace boost::filesystem;

static file_mapping try_compress_and_open(const path &p, bool &compressed)
{
	compressed = false;

	std::string ext=p.extension().string();
	if (ext=="gz" || ext=="zip"
			|| ext=="tgz" || ext=="bz2" || ext=="7z")
		return file_mapping(p.string().c_str(), read_only);

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
		int ret2=system(("gzip -n -f "+tmp_file.string()).c_str());
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
	std::vector<std::string> etags_;

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

	VLOG(2) << "Checking upload of " << path_ << " as "
			  << remote_;

	//Compute MD5
	unsigned char res[MD5_DIGEST_LENGTH+1]={0};
	MD5((const unsigned char*)up_data->region_.get_address(),
		up_data->region_.get_size(), res);
	std::string b64_md5=base64_encode((const char*)res, MD5_DIGEST_LENGTH);

	if (etag_.empty())
	{
		start_upload(agenda, b64_md5, up_data, compressed);
	} else
	{
		std::string md5_web;
		md5_web.push_back('\"');
		md5_web.append(tobinhex(res, MD5_DIGEST_LENGTH));
		md5_web.push_back('\"');
		if (md5_web == etag_)
			return;

		if (etag_.find('-')!=std::string::npos)
		{
			//Multipart upload. Get MD5 from metadata
			s3_connection up(conn_, "HEAD", remote_);
			std::string md5=up.find_md5();
			if (md5 == b64_md5)
				return;
		}

		start_upload(agenda, b64_md5, up_data, compressed);
	}
}

class part_upload_task : public sync_task
{
	size_t num_;
	std::string upload_id_;
	const connection_data conn_;
	const std::string remote_;
	upload_content_ptr content_;
public:
	part_upload_task(size_t num, const std::string &upload_id,
					 const connection_data &conn,
					 const std::string &remote, upload_content_ptr content)
		: num_(num), upload_id_(upload_id),
		  conn_(conn), remote_(remote), content_(content)
	{
	}

	virtual void operator()(agenda_ptr agenda)
	{
		VLOG(2) << "Starting upload of a part " << num_ << " of "
				<< remote_;

		header_map_t hmap;
		hmap["Content-Type"] = "application/x-binary";
		s3_connection up(conn_, "PUT", remote_+
						 "?partNumber="+int_to_string(num_+1)
						 +"&uploadId="+upload_id_,
						 hmap);

		size_t size = content_->region_.get_size();
		size_t part_size = size/content_->num_parts_;
		size_t offset = part_size*num_;
		size_t ln = size-offset;
		if (ln>part_size)
			ln=part_size;

		unsigned char *data=reinterpret_cast<unsigned char *>(
					content_->region_.get_address());
		std::string etag=up.upload_data(data+offset, ln, num_, upload_id_);
		if (etag.empty())
			err(errFatal) << "Failed to upload a part " << num_;

		VLOG(2) << "Uploaded part " << num_ << " of "
				<< remote_ << " with etag=" << etag << ".";

		guard_t g(content_->lock_);
		content_->num_completed_++;
		content_->etags_.at(num_) = etag;
		if (content_->num_completed_ == content_->num_parts_)
		{
			//We've completed the upload!
			header_map_t hmap;
			s3_connection up(conn_, "POST", remote_+
							 "?uploadId="+upload_id_, hmap);
			up.complete_multipart(content_->etags_);
		}
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
		content->num_parts_ = number_of_parts;
		content->etags_.resize(number_of_parts);

		header_map_t hmap;
		hmap["x-amz-meta-compressed"] = compressed ? "true" : "false";
		hmap["x-amz-meta-md5"] = md5;
		hmap["Content-Type"] = "application/x-binary";
		s3_connection up(conn_, "POST", remote_+"?uploads", hmap);
		std::string upload_id=up.initiate_multipart();

		for(size_t f=0;f<number_of_parts;++f)
		{
			boost::shared_ptr<part_upload_task> task(
						new part_upload_task(f, upload_id,
											 conn_, remote_, content));
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
