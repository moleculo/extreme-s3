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
#include "compressor.h"

#define MIN_PART_SIZE (16*1024*1024)
#define MIN_ALLOWED_PART_SIZE (16*1024*1024)
#define MAX_PART_NUM 10000

using namespace es3;

struct es3::upload_content
{
	upload_content() : num_parts_(), num_completed_() {}

	context_ptr conn_;
	std::string upload_id_;
	s3_path remote_;

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

	virtual void print_to(std::ostream &str)
	{
		str << "Upload segment " << num_ << " of " << content_->remote_;
	}

	virtual void operator()(agenda_ptr agenda)
	{
		VLOG(2) << "Starting upload of a part " << num_ << " of "
				<< content_->remote_;

		struct sched_param param;
		param.sched_priority = 1+num_*90/content_->num_parts_;
		pthread_setschedparam(pthread_self(), SCHED_RR, &param);

		s3_path part_path=content_->remote_;
		part_path.path_+="?partNumber="+int_to_string(num_+1)
				+"&uploadId="+content_->upload_id_;

		s3_connection up(content_->conn_);
		std::string etag=up.upload_data(part_path,
										&segment_->data_[0],
										segment_->data_.size());
		assert(!etag.empty());
		agenda->add_stat_counter("uploaded", segment_->data_.size());

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
	files_ptr files_;
	size_t cur_segment_, number_of_segments_;
	bool update_log_;
public:
	file_pump(upload_content_ptr content,
		files_ptr files, size_t cur_segment, size_t number_of_segments,
		bool update_log) :
		content_(content), files_(files),
		cur_segment_(cur_segment), number_of_segments_(number_of_segments),
		update_log_(update_log)
	{
	}

	virtual void print_to(std::ostream &str)
	{
		str << "Read segment " << cur_segment_ << " of " << content_->remote_;
	}

	virtual task_type_e get_class() const { return taskIOBound; }

	virtual size_t needs_segments() const
	{
		return number_of_segments_;
	}

	virtual void operator()(agenda_ptr agenda,
							const std::vector<segment_ptr> &segments)
	{
		size_t segment_size=agenda->segment_size();
		uint64_t start_offset = segment_size*cur_segment_;

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
			segment_ptr seg = segments.at(f);
			seg->data_.resize(segment_size, 0);

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
						std::min(segment_size-segment_read_so_far,
								 cur_piece_size-offset_within_);
				while(remaining_size>0)
				{
					char buf[65536*8];
					//No overflow is possible since sizeof(buf)<MAX_SIZE_T.
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
						assert(segment_read_so_far==segment_size);
					}
				}
			}
			assert(segment_read_so_far==segment_size
				   || f==number_of_segments_-1);

			if (update_log_)
				agenda->add_stat_counter("read", seg->data_.size());
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
	if(mod.mtime_)
	{
		if (mod.mtime_==mtime && mod.raw_size_==file_sz)
			return; //TODO: add an optional MD5 check?
	}
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
	//hmap["Content-Type"] = "application/x-binary";
	if (do_compress)
		hmap["Content-Encoding"] = "gzip";
	hmap["x-amz-meta-last-modified"] = int_to_string(mtime);
	hmap["x-amz-meta-size"] = int_to_string(file_sz);
	hmap["x-amz-meta-file-mode"] = int_to_string(mode);
	s3_connection up_prep(conn_);
	up_data->upload_id_=up_prep.initiate_multipart(remote_, hmap);

	if (do_compress)
	{
		files_finished_callback on_finish=boost::bind(
					&file_uploader::start_upload, shared_from_this(),
					agenda, up_data, _1, true);

		sync_task_ptr task(new file_compressor(path_, conn_, on_finish));
		agenda->schedule(task);
	} else
	{
		handle_t fl(open(path_.c_str(), O_RDONLY) | libc_die);
		files_ptr files(new scattered_files(path_, fl.size()));
		start_upload(agenda, up_data, files, false);
	}
}

void file_uploader::start_upload(agenda_ptr ag,
								 upload_content_ptr content,
								 files_ptr files,
								 bool compressed)
{
	uint64_t size = 0;
	for(int f=0;f<files->sizes_.size();++f)
		size+=files->sizes_.at(f);

	size_t segment_size = ag->segment_size();
	size_t number_of_segments = safe_cast<size_t>(size/segment_size +
			((size%segment_size)==0 ? 0:1));
	if (number_of_segments>MAX_PART_NUM)
		err(errFatal) << "File "<<remote_ <<" is too big";
	if (number_of_segments==0)
	{
		assert(files->sizes_.size()==1 && files->sizes_.at(0)==0);
		number_of_segments=1;
	}

	content->num_parts_ = number_of_segments;
	content->etags_.resize(number_of_segments);

	//Now create file pumps
	size_t num_per_pump = number_of_segments /
			ag->get_capability(taskIOBound) + 1;

	if (num_per_pump>ag->max_in_flight()/2)
		num_per_pump=ag->max_in_flight()/2;

	for(size_t f=0;f<number_of_segments;f+=num_per_pump)
	{
		size_t num_cur = number_of_segments-f;
		if (num_cur > num_per_pump)
			num_cur = num_per_pump;

		sync_task_ptr task(new file_pump(content, files, f, num_cur,
										 !compressed));
		ag->schedule(task);
	}
}

void remote_file_deleter::operator()(agenda_ptr agenda)
{
	VLOG(2) << "Removing " << remote_;
	s3_connection up(conn_);
	up.read_fully("DELETE", remote_);
}
