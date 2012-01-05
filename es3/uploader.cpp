#include "uploader.h"
#include "scope_guard.h"
#include "errors.h"
#include "context.h"

#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <boost/bind.hpp>
#include "workaround.hpp"
#include "compressor.h"

#define MIN_PART_SIZE (16*1024*1024)
#define MIN_ALLOWED_PART_SIZE (16*1024*1024)
#define MAX_PART_NUM 10000

using namespace es3;
using namespace boost::filesystem;

struct es3::upload_content
{
	upload_content() : num_parts_(), num_completed_() {}

	context_ptr conn_;
	std::string upload_id_;
	std::string remote_;

	std::mutex lock_;
	size_t num_parts_;
	size_t num_completed_;
	std::vector<std::string> etags_;
};

class part_upload_task : public sync_task
{
	size_t num_;
	upload_content_ptr content_;

	segment_ptr segment_;
public:
	part_upload_task(size_t num, upload_content_ptr content,
					 segment_ptr segment)
		: num_(num), content_(content), segment_(segment)
	{
	}

	virtual void operator()(agenda_ptr agenda)
	{
		VLOG(2) << "Starting upload of a part " << num_ << " of "
				<< content_->remote_;

		struct sched_param param;
		param.sched_priority = 1+num_*90/content_->num_parts_;
		pthread_setschedparam(pthread_self(), SCHED_RR, &param);

		std::string part_path=content_->remote_+
				"?partNumber="+int_to_string(num_+1)
				+"&uploadId="+content_->upload_id_;

		s3_connection up(content_->conn_);
		std::string etag=up.upload_data(part_path,
										&segment_->data_[0],
										segment_->data_.size());
		if (etag.empty())
			err(errWarn) << "Failed to upload a part " << num_;

		//Check if the upload is completed
		guard_t g(content_->lock_);
		content_->num_completed_++;
		content_->etags_.at(num_) = etag;

		VLOG(2) << "Uploaded part " << num_ << " of "<< content_->remote_
				<< " with etag=" << etag
				<< ", total=" << content_->num_parts_
				<< ", sent=" << content_->num_completed_ << ".";

		if (content_->num_completed_ == content_->num_parts_)
		{
			VLOG(2) << "Assembling "<< content_->remote_ <<".";
			//We've completed the upload!
			s3_connection up2(content_->conn_);
			up2.complete_multipart(content_->remote_, content_->upload_id_,
								   content_->etags_);
		}
	}
};

class file_pump : public sync_task,
		public boost::enable_shared_from_this<file_pump>
{
	upload_content_ptr content_;
	zip_result_ptr files_;
	size_t cur_segment_, number_of_segments_;
public:
	file_pump(upload_content_ptr content,
		zip_result_ptr files, size_t cur_segment, size_t number_of_segments) :
		content_(content), files_(files),
		cur_segment_(cur_segment), number_of_segments_(number_of_segments)
	{
	}

	virtual std::string get_class() const
	{
		return "pump"+int_to_string(get_class_limit());
	}

	virtual size_t get_class_limit() const
	{
		return content_->conn_->max_readers_;
	}

	virtual void operator()(agenda_ptr agenda)
	{
		context_ptr ctx = content_->conn_;
		uint64_t start_offset = ctx->segment_size_*cur_segment_;

		//First, skip to our piece
		uint64_t skipped_so_far=0, offset_within_=0;
		int cur_piece=0;
		for(int f=0;f<files_->sizes_.size();++f)
		{
			uint64_t cur_size = files_->sizes_.at(f);
			if (start_offset<(skipped_so_far+cur_size))
			{
				cur_piece = f;
				offset_within_ = start_offset-skipped_so_far;
				assert(offset_within_<cur_size);
				break;
			}
			skipped_so_far+=cur_size;
		}

		//Now read data!!!
		for(int f=0;f<number_of_segments_;++f)
		{
			segment_ptr seg = ctx->get_segment();
			seg->data_.resize(ctx->segment_size_, 0);

			uint64_t segment_read_so_far=0;
			while(segment_read_so_far<seg->data_.size())
			{
				//Note that we're duplicating the handle because other
				//file pumps might be using it
				handle_t cur_fl(open(
									files_->files_.at(cur_piece).c_str(), O_RDONLY));
				lseek64(cur_fl.get(), offset_within_, SEEK_SET) | libc_die;
				uint64_t cur_piece_size=files_->sizes_.at(cur_piece);

				uint64_t remaining_size =
						std::min(ctx->segment_size_-segment_read_so_far,
								 cur_piece_size-offset_within_);
				while(remaining_size>0)
				{
					char buf[65536*8];
					size_t chunk=std::min(remaining_size,
										  uint64_t(sizeof(buf)));
					size_t res=read(cur_fl.get(), buf, chunk) | libc_die;
					assert(res!=0);
					memcpy(&seg->data_[segment_read_so_far], buf, res);

					segment_read_so_far+=res;
					offset_within_+=res;
					remaining_size-=res;
				}

				//We've run out of file piece, switch to the next one
				if (cur_piece==files_->sizes_.size()-1)
				{
					//No other pieces and this is the last segment
					seg->data_.resize(segment_read_so_far);
				} else
				{
					if (cur_piece_size==offset_within_)
					{
						cur_piece++;
						offset_within_=0;
					} else
					{
						assert(segment_read_so_far==ctx->segment_size_);
					}
				}
			}
			assert(segment_read_so_far==ctx->segment_size_
				   || f==number_of_segments_-1);

			sync_task_ptr task(new part_upload_task(cur_segment_+f,
													content_, seg));
			agenda->schedule(task);
		}
	}
};

void file_uploader::operator()(agenda_ptr agenda)
{
	uint64_t file_sz=file_size(path_);
	time_t mtime=last_write_time(path_);

	struct stat stbuf={0};
	::stat(path_.c_str(), &stbuf) | libc_die;
	mode_t mode=stbuf.st_mode & 0777; //Get permissions

	//Check the modification date of the file locally and on the
	//remote side
	s3_connection up(conn_);
	file_desc mod=up.find_mtime_and_size(remote_);
	if (mod.mtime_==mtime && mod.raw_size_==file_sz)
		return; //TODO: add an optional MD5 check?
	//We don't check file mode here, because it doesn't really work
	//on Windows - we'll get permission loops.

	//Woohoo! We need to upload the file.
	upload_content_ptr up_data(new upload_content());
	up_data->conn_ = conn_;
	up_data->remote_ = remote_;

	VLOG(2) << "Starting upload of " << path_ << " as "
			  << remote_;

	bool do_compress = should_compress(path_, file_sz) &&
			conn_->do_compression_;
	//Prepare upload
	header_map_t hmap;
	hmap["x-amz-meta-compressed"] = do_compress ? "true" : "false";
	hmap["Content-Type"] = "application/x-binary";
	hmap["x-amz-meta-last-modified"] = int_to_string(mtime);
	hmap["x-amz-meta-size"] = int_to_string(file_sz);
	hmap["x-amz-meta-file-mode"] = int_to_string(mode);
	s3_connection up_prep(conn_);
	up_data->upload_id_=up_prep.initiate_multipart(remote_, hmap);

	if (do_compress)
	{
		zipped_callback on_finish=boost::bind(
					&file_uploader::start_upload, shared_from_this(),
					agenda, up_data, _1, true);

		sync_task_ptr task(new file_compressor(path_, conn_, on_finish));
		agenda->schedule(task);
	} else
	{
		handle_t fl(open(path_.c_str(), O_RDONLY) | libc_die);
		zip_result_ptr files(new compressed_result(path_, fl.size()));
		start_upload(agenda, up_data, files, false);
	}
}

void file_uploader::start_upload(agenda_ptr ag,
								 upload_content_ptr content,
								 zip_result_ptr files,
								 bool compressed)
{
	uint64_t size = 0;
	for(int f=0;f<files->sizes_.size();++f)
		size+=files->sizes_.at(f);

	size_t number_of_segments= size/conn_->segment_size_ +
			((size%conn_->segment_size_)==0 ? 0:1);
	if (number_of_segments>MAX_PART_NUM)
		err(errFatal) << "File "<<remote_ <<" is too big";

	content->num_parts_ = number_of_segments;
	content->etags_.resize(number_of_segments);

	//Now create file pumps
	size_t num_per_pump = number_of_segments / conn_->max_readers_ + 1;
	for(int f=0;f<number_of_segments;f+=num_per_pump)
	{
		size_t num_cur = number_of_segments-f;
		if (num_cur > num_per_pump)
			num_cur = num_per_pump;

		sync_task_ptr task(new file_pump(content, files, f, num_cur));
		ag->schedule(task);
	}
}

void file_deleter::operator()(agenda_ptr agenda)
{
	VLOG(2) << "Removing " << remote_;
	s3_connection up(conn_);
	up.read_fully("DELETE", remote_);
}
