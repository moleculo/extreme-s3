#include "uploader.h"
#include "scope_guard.h"
#include "errors.h"

#include <iostream>
#include <openssl/md5.h>
#include <stdint.h>
#include "workaround.hpp"

//Workaround for Boost bugs
#undef BOOST_HAS_RVALUE_REFS
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#define COMPRESSION_THRESHOLD 10000000
#define MIN_RATIO 0.9d

#define MIN_PART_SIZE 5242880
#define MIN_ALLOWED_PART_SIZE 5242880
#define MAX_PART_NUM 200

using namespace es3;
using namespace boost::interprocess;
using namespace boost::filesystem;

static bool should_compress(const std::string &p,
							const mapped_region &region, uint64_t sz)
{
	return false;
	std::string ext=get_file(path(p).extension());
	if (ext==".gz" || ext==".zip" ||
			ext==".tgz" || ext==".bz2" || ext==".7z")
		return false;

	if (sz <= COMPRESSION_THRESHOLD)
		return false;

	//Check for GZIP magic
	assert(region.get_size()>4);
	const unsigned char *magic =
			reinterpret_cast<const unsigned char *>(region.get_address());
	if (magic[0]==0x1F && magic[1] == 0x8B && magic[2]==0x8 && magic[3]==0x8)
		return false;
	return true;
}

struct es3::upload_content
{
	upload_content() : num_parts_(), num_completed_() {}

	std::mutex lock_;
	size_t num_parts_;
	size_t num_completed_;
	std::vector<std::string> etags_;
	time_t mtime_;

	file_mapping mapping_;
	mapped_region region_;
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
	up_data->mtime_ = mtime;
	up_data->mapping_ = file_mapping(path_.c_str(), read_only);

	//Check for empty files - we must do it here explicitely because mapping
	//a zero-length file causes an exception to be thrown
	offset_t region_size;
	boost::interprocess::detail::get_file_size(
				up_data->mapping_.get_mapping_handle().handle, region_size);
	if (region_size==0)
	{
//		start_upload("", "", 0, false);
		//We do not upload empty files.
		//TODO: ??
		return;
	}

	//Map the whole file in this process
	up_data->region_ = mapped_region(up_data->mapping_,read_only);

	VLOG(2) << "Starting upload of " << path_ << " as "
			  << remote_;
	if (region_size<=MIN_PART_SIZE*2)
	{
		simple_upload(agenda, up_data);
	} else
	{
		//Rest in pieces!
		bool do_compress = should_compress(path_, up_data->region_,
										   region_size);
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

	std::pair<size_t, std::string> res=up.upload_data(remote_,
					content->region_.get_address(),
					content->region_.get_size(),
					false, false, hmap);
	assert(res.first==content->region_.get_size());
}

class part_upload_task : public sync_task
{
	size_t num_;
	std::string upload_id_;
	const connection_data conn_;
	const std::string remote_;
	upload_content_ptr content_;
	bool compressed_;
public:
	part_upload_task(size_t num, const std::string &upload_id,
					 const connection_data &conn,
					 const std::string &remote, upload_content_ptr content,
					 bool compressed)
		: num_(num), upload_id_(upload_id),
		  conn_(conn), remote_(remote), content_(content),
		  compressed_(compressed)
	{
	}

	virtual void operator()(agenda_ptr agenda)
	{
		VLOG(2) << "Starting upload of a part " << num_ << " of "
				<< remote_;

		struct sched_param param;
		param.sched_priority = num_*90/content_->num_parts_;
		pthread_setschedparam(getpid(), SCHED_RR, &param);

		size_t size = content_->region_.get_size();
		size_t part_size = size/content_->num_parts_;
		size_t offset = part_size*num_;
		size_t ln = size-offset;
		if (ln>part_size)
			ln=part_size;
		unsigned char *data=reinterpret_cast<unsigned char *>(
					content_->region_.get_address());

		std::string part_path=remote_+
				"?partNumber="+int_to_string(num_+1)
				+"&uploadId="+upload_id_;
		header_map_t hmap;
		hmap["Content-Type"] = "application/x-binary";

		s3_connection up(conn_);
		std::pair<size_t,std::string> res=up.upload_data(part_path,
					data+offset, ln, compressed_, MIN_ALLOWED_PART_SIZE, hmap);
		assert(res.first>=MIN_ALLOWED_PART_SIZE ||
			   num_==content_->num_parts_-1);
		std::string etag = res.second;

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
			s3_connection up2(conn_);
			up2.complete_multipart(remote_+"?uploadId="+upload_id_,
								  content_->etags_);
		}
	}
};

void file_uploader::start_upload(agenda_ptr ag,
								 upload_content_ptr content, bool compressed)
{
	VLOG(2) << "Starting upload of " << path_ << " as "
			<< remote_ ;

	size_t size=content->region_.get_size();

	size_t number_of_parts = size/MIN_PART_SIZE;
	if (number_of_parts>MAX_PART_NUM)
		number_of_parts = MAX_PART_NUM;
	content->num_parts_ = number_of_parts;
	content->etags_.resize(number_of_parts);

	header_map_t hmap;
	hmap["x-amz-meta-compressed"] = compressed ? "true" : "false";
	hmap["Content-Type"] = "application/x-binary";
	hmap["x-amz-meta-last-modified"] = int_to_string(content->mtime_);
	s3_connection up(conn_);
	std::string upload_id=up.initiate_multipart(remote_, hmap);

	for(size_t f=0;f<number_of_parts;++f)
	{
		boost::shared_ptr<part_upload_task> task(
					new part_upload_task(f, upload_id,
										 conn_, remote_, content, compressed));
		ag->schedule(task);
	}
}

void file_deleter::operator()(agenda_ptr agenda)
{
	VLOG(2) << "Removing " << remote_;
	s3_connection up(conn_);
	up.read_fully("DELETE", remote_);
}
