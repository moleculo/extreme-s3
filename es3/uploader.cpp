#include "uploader.h"
#include "scope_guard.h"
#include "errors.h"

#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include "workaround.hpp"

#define COMPRESSION_THRESHOLD 10000000
#define MIN_RATIO 0.9d

#define MIN_PART_SIZE (6*1024*1024)
#define MIN_ALLOWED_PART_SIZE (6*1024*1024)
#define MAX_PART_NUM 10000

using namespace es3;
using namespace boost::filesystem;

static bool should_compress(const std::string &p, uint64_t sz)
{
	return false;
	std::string ext=get_file(path(p).extension());
	if (ext==".gz" || ext==".zip" ||
			ext==".tgz" || ext==".bz2" || ext==".7z")
		return false;

	if (sz <= COMPRESSION_THRESHOLD)
		return false;

	//Check for GZIP magic
	int fl=open(p.c_str(), O_RDONLY) | libc_die;
	ON_BLOCK_EXIT(&close, fl);

	char magic[4]={0};
	read(fl, magic, 4) | libc_die;
	if (magic[0]==0x1F && magic[1] == 0x8B && magic[2]==0x8 && magic[3]==0x8)
		return false;
	return true;
}

struct es3::upload_content
{
	upload_content() : num_parts_(), num_completed_() {}

	std::mutex lock_;
	connection_data conn_;
	time_t mtime_;

	size_t num_parts_;
	size_t num_completed_;
	std::vector<std::string> etags_;
};

void file_uploader::operator()(agenda_ptr agenda)
{
	uint64_t file_sz=file_size(path_);
	time_t mtime=last_write_time(path_);

	//Check the modification date of the file locally and on the
	//remote side
	s3_connection up(conn_);
	std::pair<time_t, uint64_t> mod=up.find_mtime_and_size(remote_);
	if (mod.first==mtime && mod.second==file_sz)
		return; //TODO: add an optional MD5 check?

	//Woohoo! We need to upload the file.
	upload_content_ptr up_data(new upload_content());
	up_data->conn_ = conn_;
	up_data->mtime_ = mtime;

	VLOG(2) << "Starting upload of " << path_ << " as "
			  << remote_;
	if (file_sz<=MIN_PART_SIZE*2)
	{
		simple_upload(agenda, up_data);
	} else
	{
		//Rest in pieces!
		bool do_compress = should_compress(path_, file_sz);
		start_upload(agenda, up_data, do_compress);
	}
}

void file_uploader::simple_upload(agenda_ptr ag, upload_content_ptr content)
{
	//Simple upload
	header_map_t hmap;
	hmap["x-amz-meta-compressed"] = "false";
	hmap["Content-Type"] = "application/x-binary";
	hmap["x-amz-meta-last-modified"] = int_to_string(content->mtime_);
	s3_connection up(conn_);

	handle_t file(open(path_.c_str(), O_RDONLY));
	int64_t sz=lseek64(file.get(), 0, SEEK_END);
	hmap["x-amz-meta-size"] = int_to_string(sz);
	std::string etag=up.upload_data(remote_,
									file, sz, 0, hmap);
}

class part_upload_task : public sync_task
{
	size_t num_;
	std::string upload_id_;

	const std::string remote_;
	upload_content_ptr content_;

	handle_t descriptor_;
	uint64_t offset_, size_;
public:
	part_upload_task(size_t num, const std::string &upload_id,
					 const std::string &remote,
					 upload_content_ptr content,
					 handle_t descriptor, uint64_t offset, uint64_t size)
		: num_(num), upload_id_(upload_id),
		  remote_(remote), content_(content),
		  descriptor_(descriptor), offset_(offset), size_(size)
	{
	}

	virtual void operator()(agenda_ptr agenda)
	{
		VLOG(2) << "Starting upload of a part " << num_ << " of "
				<< remote_;

		struct sched_param param;
		param.sched_priority = 1+num_*90/content_->num_parts_;
		pthread_setschedparam(pthread_self(), SCHED_RR, &param);

		std::string part_path=remote_+
				"?partNumber="+int_to_string(num_+1)
				+"&uploadId="+upload_id_;
		header_map_t hmap;
		hmap["Content-Type"] = "application/x-binary";

		s3_connection up(content_->conn_);
		std::string etag=up.upload_data(part_path, descriptor_, size_, offset_,
										hmap);
		if (etag.empty())
			err(errWarn) << "Failed to upload a part " << num_;

		//Check if the upload is completed
		guard_t g(content_->lock_);
		content_->num_completed_++;
		content_->etags_.at(num_) = etag;

		VLOG(2) << "Uploaded part " << num_ << " of "<< remote_
				<< " with etag=" << etag
				<< ", total=" << content_->num_parts_
				<< ", sent=" << content_->num_completed_ << ".";

		if (content_->num_completed_ == content_->num_parts_)
		{
			VLOG(2) << "Assembling "<< remote_ <<".";
			//We've completed the upload!
			s3_connection up2(content_->conn_);
			up2.complete_multipart(remote_+"?uploadId="+upload_id_,
								  content_->etags_);
		}
	}
};

void file_uploader::start_upload(agenda_ptr ag,
								 upload_content_ptr content, bool compressed)
{
	handle_t file(open(path_.c_str(), O_RDONLY));
	uint64_t size = lseek64(file.get(), 0, SEEK_END);

	size_t number_of_parts = size/MIN_PART_SIZE;
	if (number_of_parts>MAX_PART_NUM)
		number_of_parts = MAX_PART_NUM;
	content->num_parts_ = number_of_parts;
	content->etags_.resize(number_of_parts);

	header_map_t hmap;
	hmap["x-amz-meta-compressed"] = compressed ? "true" : "false";
	hmap["Content-Type"] = "application/x-binary";
	hmap["x-amz-meta-last-modified"] = int_to_string(content->mtime_);
	hmap["x-amz-meta-size"] = int_to_string(size);

	s3_connection up(conn_);
	std::string upload_id=up.initiate_multipart(remote_, hmap);

	for(size_t f=0;f<number_of_parts;++f)
	{
		uint64_t offset = (size/number_of_parts)*f;
		uint64_t cur_size = size/number_of_parts;
		if (size-offset < cur_size)
			cur_size = size- offset;

		boost::shared_ptr<part_upload_task> task(
					new part_upload_task(f, upload_id,
										 remote_, content, file.dup(),
										 offset, cur_size));
		ag->schedule(task);
	}
}

void file_deleter::operator()(agenda_ptr agenda)
{
	VLOG(2) << "Removing " << remote_;
	s3_connection up(conn_);
	up.read_fully("DELETE", remote_);
}
