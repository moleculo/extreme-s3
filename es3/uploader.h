#ifndef UPLOADER_H
#define UPLOADER_H

#include "connection.h"
#include "common.h"
#include "agenda.h"

namespace es3 {
	struct upload_content;
	typedef boost::shared_ptr<upload_content> upload_content_ptr;
	struct scattered_files;
	typedef boost::shared_ptr<scattered_files> files_ptr;

	class file_uploader : public sync_task,
			public boost::enable_shared_from_this<file_uploader>
	{
		const context_ptr conn_;
		const bf::path path_;
		const s3_path remote_;
		const bool just_touch_;
	public:
		file_uploader(const context_ptr &conn,
					  const bf::path &path,
					  const s3_path &remote,
					  bool just_touch=false)
			: conn_(conn), path_(path), remote_(remote), just_touch_(just_touch)
		{
		}

		virtual void operator()(agenda_ptr agenda);
		virtual void print_to(std::ostream &str)
		{
			str << "Upload " << path_ << " to " << remote_;
		}

	private:
		void start_upload(agenda_ptr ag,
						  upload_content_ptr content, files_ptr files,
						  bool compressed);
		void simple_upload(agenda_ptr ag, upload_content_ptr content);
	};

	class remote_file_deleter : public sync_task
	{
		const context_ptr conn_;
		const s3_path remote_;
	public:
		remote_file_deleter(const context_ptr &conn, const s3_path &remote)
			: conn_(conn), remote_(remote)
		{
		}

		virtual void operator()(agenda_ptr agenda);
		virtual void print_to(std::ostream &str)
		{
			str << "Delete " << remote_;
		}

	private:
	};

}; //namespace es3

#endif //UPLOADER_H
