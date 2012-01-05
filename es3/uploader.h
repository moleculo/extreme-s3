#ifndef UPLOADER_H
#define UPLOADER_H

#include "connection.h"
#include "common.h"
#include "agenda.h"

namespace es3 {
	struct upload_content;
	typedef boost::shared_ptr<upload_content> upload_content_ptr;
	struct compressed_result;
	typedef boost::shared_ptr<compressed_result> zip_result_ptr;

	class file_uploader : public sync_task,
			public boost::enable_shared_from_this<file_uploader>
	{
		const context_ptr conn_;
		const std::string path_;
		const std::string remote_;
	public:
		file_uploader(const context_ptr &conn,
					  const bf::path &path,
					  const std::string &remote)
			: conn_(conn), path_(path.string()), remote_(remote)
		{
		}

		virtual void operator()(agenda_ptr agenda);
	private:
		void start_upload(agenda_ptr ag,
						  upload_content_ptr content, zip_result_ptr files,
						  bool compressed);
		void simple_upload(agenda_ptr ag, upload_content_ptr content);
	};

	class remote_file_deleter : public sync_task
	{
		const context_ptr conn_;
		const std::string remote_;
	public:
		remote_file_deleter(const context_ptr &conn, const std::string &remote)
			: conn_(conn), remote_(remote)
		{
		}

		virtual void operator()(agenda_ptr agenda);
	private:
	};

}; //namespace es3

#endif //UPLOADER_H
