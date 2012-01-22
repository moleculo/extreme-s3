#ifndef SYNC_H
#define SYNC_H

#include <common.h>
#include "agenda.h"
#include "connection.h"
#include <stdint.h>

namespace es3 {
	struct local_file;
	struct local_dir;
	typedef boost::shared_ptr<local_file> local_file_ptr;
	typedef boost::shared_ptr<local_dir> local_dir_ptr;

	class synchronizer
	{
		agenda_ptr agenda_;
		context_ptr ctx_;
		std::vector<s3_path> remote_;
		stringvec local_;
		bool do_upload_;
		bool delete_missing_;
		stringvec included_, excluded_;
	public:
		synchronizer(agenda_ptr agenda, const context_ptr &ctx,
					 std::vector<s3_path> remote, stringvec local,
					 bool do_upload, bool delete_missing,
					 const stringvec &included, const stringvec &excluded);
		void create_schedule();

		bool check_included(const std::string &name);
	private:
		void process_upload(local_dir_ptr locals, s3_directory_ptr remotes,
							const s3_path &remote_path);
		void process_downloads(s3_directory_ptr remotes, local_dir_ptr locals);

		void delete_recursive(s3_directory_ptr dir);
	};

}; //namespace es3

#endif //UPLOADER_H
