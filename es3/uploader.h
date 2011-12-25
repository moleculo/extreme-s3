#ifndef UPLOADER_H
#define UPLOADER_H

#include <common.h>
#include "agenda.h"
#include "connection.h"
#include <stdint.h>
#include <boost/filesystem.hpp>

namespace es3 {

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
		void start_upload(const std::string &md5, void *addr, size_t size,
						  bool compressed);
	};
}; //namespace es3

#endif //UPLOADER_H
