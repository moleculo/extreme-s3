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
		std::string remote_;
		bf::path local_;
		bool do_upload_;
		bool delete_missing_;
	public:
		synchronizer(agenda_ptr agenda, const context_ptr &ctx,
					 std::string remote, bf::path local,
					 bool do_upload, bool delete_missing);
		void create_schedule();

	private:
		void process_dir(file_map_t *remote_list,
						 const std::string &remote_dir,
						 const bf::path &local_dir);

		void process_missing(const file_map_t &cur,
							 const bf::path &cur_local_dir);
	};

}; //namespace es3

#endif //UPLOADER_H
