#ifndef UPLOADER_H
#define UPLOADER_H

#include "connection.h"
#include "common.h"
#include "agenda.h"
#include <boost/filesystem.hpp>

namespace es3 {
	struct upload_content;
	typedef boost::shared_ptr<upload_content> upload_content_ptr;

	class file_uploader : public sync_task
	{
		const connection_data conn_;
		const boost::filesystem::path path_;
		const std::string remote_;
		const std::string etag_;

	public:
		file_uploader(const connection_data &conn,
					  const boost::filesystem::path &path,
					  const std::string &remote,
					  const std::string &etag)
			: conn_(conn), path_(path), remote_(remote), etag_(etag)
		{
		}

		virtual void operator()(agenda_ptr agenda);
	private:
		void start_upload(agenda_ptr ag, const std::string &md5,
						  upload_content_ptr content, bool compressed);
	};

	class file_deleter : public sync_task
	{
		const connection_data conn_;
		const std::string remote_;
	public:
		file_deleter(const connection_data &conn,
					  const std::string &remote)
			: conn_(conn), remote_(remote)
		{
		}

		virtual void operator()(agenda_ptr agenda);
	private:
	};

}; //namespace es3

#endif //UPLOADER_H
