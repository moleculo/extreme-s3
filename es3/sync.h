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
		context_ptr to_;
		std::string cur_remote_;
	public:
		synchronizer(agenda_ptr agenda, const context_ptr &to);
		void create_schedule();

	private:
		void process_dir(file_map_t *cur_remote,
						 const std::string &cur_local,
						 const std::string &cur_remote_path);

		void process_missing(const file_map_t &cur,
							 const std::string &cur_remote_path);
	};

}; //namespace es3

#endif //UPLOADER_H
