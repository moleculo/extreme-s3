#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include "connection.h"
#include "common.h"
#include "agenda.h"

namespace es3 {

	class file_downloader : public sync_task
	{
		const context_ptr conn_;
		const bool delete_dir_;
		const std::string path_;
		const std::string remote_;

	public:
		file_downloader(const context_ptr &conn,
					  const std::string &path,
					  const std::string &remote,
					  bool delete_dir = false)
			: conn_(conn), path_(path), remote_(remote),
			  delete_dir_(delete_dir)
		{
		}

		virtual void operator()(agenda_ptr agenda);
	private:
	};

	class local_file_deleter : public sync_task
	{
		const std::string file_;
	public:
		local_file_deleter(const std::string &file)
			: file_(file)
		{
		}
		virtual void operator()(agenda_ptr agenda);
	private:
	};

}; //namespace es3

#endif //DOWNLOADER_H
