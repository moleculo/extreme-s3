#include "compressor.h"
#include <zlib.h>
#include "scope_guard.h"
#include "workaround.hpp"
#include "errors.h"
#include <stdio.h>
#include <fcntl.h>

using namespace es3;
using namespace boost::filesystem;

#define MINIMAL_BLOCK (1024*1024)

namespace es3
{
	struct compress_task : public sync_task
	{
		compressor_ptr parent_;
		uint64_t block_num_, block_total_, total_sz_;

		virtual void operator()(agenda_ptr agenda)
		{
			handle_t src(open(parent_->path_.c_str(), O_RDONLY));

			//Generate the temp name
			path tmp_nm = path(parent_->context_->scratch_path_) /
					unique_path("scratchy-%%%%-%%%%");
			handle_t tmp_desc(open(tmp_nm.c_str(), O_RDWR|O_CREAT));

			uint64_t offset = (total_sz_/block_total_)*block_num_;
			uint64_t size = total_sz_/block_total_;
			if (total_sz_-offset < size)
				size = total_sz_-offset;
			//Seek pos
			lseek64(src.get(), offset, SEEK_SET) | libc_die;

			VLOG(2) << "Compressing part " << block_num_ << " out of " <<
					   block_total_ << " of " << parent_->path_;

			z_stream stream = {0};
			deflateInit2(&stream, 8, Z_DEFLATED,
							   15|16, //15 window bits | GZIP
							   8,
							   Z_DEFAULT_STRATEGY);
			ON_BLOCK_EXIT(&deflateEnd, &stream);

			char buf[65536*4];
			char buf_out[65536];
			size_t consumed=0;
			while(true)
			{
				ssize_t ln=read(src.get(), buf, sizeof(buf));
				if (ln<=0)
					break;

				stream.avail_in = ln;
				stream.next_in = (Bytef*)buf;

				do
				{
					stream.avail_out= sizeof(buf_out);
					stream.next_out = (Bytef*)buf_out;
					int c_err=deflate(&stream, Z_NO_FLUSH);
					if (c_err!=Z_OK)
						err(errFatal) << "Failed to compress "
									  << parent_->path_;

					size_t cur_consumed=sizeof(buf_out) - stream.avail_out;
					write(tmp_desc.get(), buf_out, cur_consumed) | libc_die;
					consumed += cur_consumed;
				} while(stream.avail_in!=0);
			}

			stream.avail_out= sizeof(buf_out);
			stream.next_out = (Bytef*)buf_out;
			int c_err=deflate(&stream, Z_FINISH); //We're writing the epilogue
			if (c_err!=Z_STREAM_END)
				err(errFatal) << "Failed to finish compression of "
							  << parent_->path_;
			size_t cur_consumed=sizeof(buf_out) - stream.avail_out;
			consumed += cur_consumed;
			write(tmp_desc.get(), buf_out, cur_consumed) | libc_die;

			parent_->on_complete(tmp_nm.c_str(), block_num_, consumed);
		}
	};
}; //namespace es3

void file_compressor::operator()(agenda_ptr agenda)
{
	uint64_t file_sz=file_size(path_);
	if (file_sz<=MINIMAL_BLOCK)
	{
		handle_t desc(open(path_.c_str(), O_RDONLY));
		on_finish_(desc, file_sz);
		return;
	}

	//Start compressing
	uint64_t num_blocks = file_sz / MINIMAL_BLOCK;
	if (num_blocks>get_class_limit())
		num_blocks = get_class_limit();

	result_=zip_result_ptr(new compressed_result(num_blocks));
	num_pending_ = num_blocks;
	for(uint64_t f=0; f<num_blocks; ++f)
	{
		boost::shared_ptr<compress_task> ptr(new compress_task());
		ptr->parent_=shared_from_this();
		ptr->block_num_=f;
		ptr->block_total_=num_blocks;
		ptr->total_sz_=file_sz;
		agenda->schedule(ptr);
	}
}

void file_compressor::on_complete(const std::string &name, uint64_t num,
				 uint64_t resulting_size)
{
	{
		guard_t lock(m_);

		num_pending_--;
		result_->files_.at(num) = name;
		result_->sizes_.at(num) = resulting_size;
	}
	if (num_pending_==0)
	{
//		on_finish_(tmp_desc, total);
	}
}
