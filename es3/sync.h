#ifndef SYNC_H
#define SYNC_H

#include <common.h>
#include "agenda.h"
#include "connection.h"
#include <stdint.h>

namespace es3 {

	class synchronizer
	{
		agenda_ptr agenda_;
		context_ptr ctx_;
		stringvec remote_;
		std::vector<bf::path> local_;
		bool do_upload_;
		bool delete_missing_;
		stringvec included_, excluded_;
	public:
		synchronizer(agenda_ptr agenda, const context_ptr &ctx,
					 stringvec remote, std::vector<bf::path> local,
					 bool do_upload, bool delete_missing,
					 const stringvec &included, const stringvec &excluded);
		void create_schedule();

	private:
		void process_dir(file_map_t *remote_list,
						 const std::string &remote_dir,
						 const bf::path &local_dir);

		void process_missing(const file_map_t &cur,
							 const bf::path &cur_local_dir);
		bool check_included(const std::string &name);
	};

}; //namespace es3

#endif //UPLOADER_H
