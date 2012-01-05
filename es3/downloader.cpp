#include "downloader.h"
#include "workaround.hpp"
#include "compressor.h"
#include "context.h"
#include "errors.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>

using namespace es3;
using namespace boost::filesystem;

void local_file_deleter::operator ()(agenda_ptr agenda_)
{
	VLOG(1) << "Removing " << file_;
	bf::remove_all(file_);
}

struct download_content
{
	std::mutex m_;
	context_ptr ctx_;

	time_t mtime_;
	mode_t mode_;
	size_t num_segments_, segments_read_;
	size_t remote_size_, raw_size_;

	std::string remote_path_,local_file_, target_file_;
	bool delete_temp_file_;

	download_content() : mtime_(), num_segments_(), segments_read_(),
		remote_size_(), raw_size_(), delete_temp_file_(true),
		mode_(0664) {}
	~download_content()
	{
		if (local_file_!=target_file_ && delete_temp_file_)
			unlink(local_file_.c_str());
	}
};
typedef boost::shared_ptr<download_content> download_content_ptr;

class write_segment_task: public sync_task,
		public boost::enable_shared_from_this<write_segment_task>
{
	download_content_ptr content_;
	size_t cur_segment_;
	segment_ptr seg_;
public:
	write_segment_task(download_content_ptr content,
					   size_t cur_segment, segment_ptr seg) :
		content_(content), cur_segment_(cur_segment), seg_(seg)
	{
	}

	virtual std::string get_class() const
	{
		return "writer"+int_to_string(get_class_limit());
	}
	virtual size_t get_class_limit() const
	{
		return content_->ctx_->max_readers_;
	}

	virtual void operator()(agenda_ptr agenda)
	{
		context_ptr ctx = content_->ctx_;
		do_write();

		guard_t lock(content_->m_);
		content_->segments_read_++;
		if (content_->segments_read_==content_->num_segments_)
		{
			//Check if we need to decompress the file
			if (content_->local_file_!=content_->target_file_)
			{
				//Yep, we do need to decompress it
				sync_task_ptr dl(new file_decompressor(ctx,
					content_->local_file_, content_->target_file_,
					content_->mtime_, content_->mode_, true));
				content_->delete_temp_file_=false;
				agenda->schedule(dl);
			} else
			{
				chmod(content_->target_file_.c_str(), content_->mode_);
				last_write_time(content_->target_file_, content_->mtime_) ;
			}
		}
	}

	void do_write()
	{
		context_ptr ctx = content_->ctx_;
		uint64_t start_offset = ctx->segment_size_*cur_segment_;
		handle_t fl(open(content_->local_file_.c_str(), O_RDWR) | libc_die);
		lseek64(fl.get(), start_offset, SEEK_SET) | libc_die;

		size_t offset = 0;
		while(offset<seg_->data_.size())
		{
			size_t chunk=std::min(seg_->data_.size()-offset, size_t(1024*1024));
			size_t res=write(fl.get(), &seg_->data_[offset],
							 chunk) | libc_die;
			assert(res!=0);
			offset+=res;
		}
	}
};

class download_segment_task: public sync_task,
		public boost::enable_shared_from_this<download_segment_task>
{
	download_content_ptr content_;
	size_t cur_segment_;
public:
	download_segment_task(download_content_ptr content, size_t cur_segment) :
		content_(content), cur_segment_(cur_segment)
	{
	}

	virtual void operator()(agenda_ptr agenda)
	{
		context_ptr ctx = content_->ctx_;
		segment_ptr seg=ctx->get_segment();

		uint64_t start_offset = ctx->segment_size_*cur_segment_;
		uint64_t size = content_->remote_size_-start_offset;
		if (size>ctx->segment_size_)
			size=ctx->segment_size_;

		VLOG(2) << "Downloading part " << cur_segment_ << " out of "
				<< content_->num_segments_ << " of " << content_->remote_path_;

		s3_connection conn(ctx);
		seg->data_.resize(size);
		conn.download_data(content_->remote_path_, start_offset,
						   &seg->data_[0], size);

		VLOG(2) << "Finished downloading part " << cur_segment_ << " out of "
				<< content_->num_segments_ << " of " << content_->remote_path_;

		//Now write the resulting segment
		sync_task_ptr dl(new write_segment_task(content_, cur_segment_, seg));
		agenda->schedule(dl);
	}
};

void file_downloader::operator()(agenda_ptr agenda)
{
	VLOG(2) << "Checking download of " << path_ << " from "
			  << remote_;

	uint64_t file_sz=0;
	time_t mtime=0;
	try
	{
		file_sz=file_size(path_);
		mtime=last_write_time(path_);
	} catch(const boost::filesystem3::filesystem_error&) {}

	//Check the modification date of the file locally and on the
	//remote side
	s3_connection up(conn_);
	file_desc mod=up.find_mtime_and_size(remote_);
	if (mod.mtime_==mtime && mod.raw_size_==file_sz)
		return; //TODO: add an optional MD5 check?

	//We need to download the file
	download_content_ptr dc(new download_content());
	dc->ctx_=conn_;

	size_t seg_num = mod.remote_size_/conn_->segment_size_ +
			((mod.remote_size_%conn_->segment_size_)==0?0:1);
	if (seg_num>MAX_SEGMENTS)
		err(errFatal) << "Segment size is too small for " << remote_;

	dc->mtime_=mod.mtime_;
	dc->mode_=mod.mode_;
	dc->num_segments_=seg_num;
	dc->segments_read_=0;
	dc->remote_size_=mod.remote_size_;
	dc->raw_size_=mod.raw_size_;

	dc->remote_path_=remote_;
	dc->target_file_=path_;

	VLOG(2) << "Downloading " << path_ << " from " << remote_;

	if (mod.compressed)
	{
		path tmp_nm = path(conn_->scratch_path_) /
				unique_path("scratchy-%%%%-%%%%-%%%%-%%%%-dl");
		dc->local_file_=tmp_nm.c_str();
	} else
		dc->local_file_=path_;

	{
		unlink(dc->local_file_.c_str()); //Prevent some access right foulups
		handle_t fl(open(dc->local_file_.c_str(),
						 O_RDWR|O_CREAT, 0600) | libc_die);
		fallocate64(fl.get(), 0, 0, dc->remote_size_);
	}

	for(size_t f=0;f<seg_num;++f)
	{
		sync_task_ptr dl(new download_segment_task(dc, f));
		agenda->schedule(dl);
	}
}
