#ifndef AGENDA_H
#define AGENDA_H

#include "common.h"
#include "errors.h"
#include <boost/filesystem.hpp>

namespace es3 {
	struct connection_data;
	struct remote_file;
	typedef boost::shared_ptr<remote_file> remote_file_ptr;
	class agenda;
	typedef boost::shared_ptr<agenda> agenda_ptr;

	class sync_task
	{
	public:
		virtual ~sync_task() {}
		virtual void operator()(agenda_ptr agenda) = 0;
	};
	typedef boost::shared_ptr<sync_task> sync_task_ptr;

	class agenda : public boost::enable_shared_from_this<agenda>
	{
		agenda();
	public:
		static boost::shared_ptr<agenda> make_new();

		void schedule(sync_task_ptr task);

		void schedule_removal(const connection_data &data,
							  remote_file_ptr file);
		void schedule_upload(const connection_data &data,
			const boost::filesystem::path &path, const std::string &remote,
			const std::string &etag);
	};

}; //namespace es3

#endif //AGENDA_H
