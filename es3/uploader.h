#ifndef UPLOADER_H
#define UPLOADER_H

#include <common.h>
#include "agenda.h"
#include "connection.h"
#include <stdint.h>
#include <boost/filesystem.hpp>

namespace es3 {

	class upload_task : public sync_task
	{
		agenda_ptr agenda_;
		boost::filesystem::path from_;
		connection_data to_;
		std::string cur_remote_;
	public:
		upload_task(agenda_ptr agenda, const boost::filesystem::path &from,
					const connection_data &to, const std::string &cur_remote);

		virtual ~upload_task();
		virtual result_code_t operator()();
		virtual std::string describe() const;
	};

}; //namespace es3

#endif //UPLOADER_H
