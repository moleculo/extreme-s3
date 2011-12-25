#ifndef SYNC_H
#define SYNC_H

#include <common.h>
#include "agenda.h"
#include "connection.h"
#include <stdint.h>
#include <boost/filesystem.hpp>

namespace es3 {

	class synchronizer
	{
		agenda_ptr agenda_;
		connection_data to_;
		std::string cur_remote_;
	public:
		synchronizer(agenda_ptr agenda, const connection_data &to);
		void create_schedule();

	private:
		void process_dir(file_map_t *cur_remote, const
						 boost::filesystem::path &cur_local,
						 const std::string &cur_remote_path);

		void del_recursive(const file_map_t &cur);
	};

}; //namespace es3

#endif //UPLOADER_H
