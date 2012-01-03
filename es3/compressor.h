#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "common.h"
#include "agenda.h"
#include <functional>

namespace es3 {

	struct compressed_result
	{
		std::vector<handle_t> descriptors_;
		std::vector<uint64_t> sizes_;
		bool was_compressed_;

		compressed_result(size_t sz)
		{
			descriptors_.resize(sz);
			sizes_.resize(sz);
			was_compressed_ = true;
		}

		compressed_result(const handle_t &desc, uint64_t sz)
		{
			descriptors_.push_back(desc);
			sizes_.push_back(sz);
			was_compressed_=false;
		}
	};
	typedef boost::shared_ptr<compressed_result> zip_result_ptr;
	typedef std::function<void(zip_result_ptr)> zipped_callback;

	class file_compressor : public sync_task,
			public boost::enable_shared_from_this<file_compressor>
	{
		std::mutex m_;

		const std::string path_, scratch_path_;
		zipped_callback on_finish_;

		zip_result_ptr result_;
		volatile size_t num_pending_;

		friend struct compress_task;
	public:
		file_compressor(const std::string &path,
						const std::string &scratch_path,
						zipped_callback on_finish)
			: path_(path), scratch_path_(scratch_path), on_finish_(on_finish)
		{
		}

		virtual std::string get_class() const { return "compression"; }
		virtual int get_class_limit() const { return 8; }
		virtual void operator()(agenda_ptr agenda);
	private:
		void on_complete(const handle_t &descriptor, uint64_t num,
						 uint64_t resulting_size);
	};

	typedef boost::shared_ptr<file_compressor> compressor_ptr;

}; //namespace es3

#endif //COMPRESSOR_H
