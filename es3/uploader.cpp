#include "uploader.h"
#include <set>
#include <boost/filesystem.hpp>
#include <iostream>

using namespace es3;
using namespace boost::filesystem;

upload_task::upload_task(agenda_ptr agenda, const path &from,
						 const connection_data &to,
						 const std::string &cur_remote)
	: agenda_(agenda), from_(from), to_(to), cur_remote_(cur_remote)
{

}

upload_task::~upload_task()
{
}

result_code_t upload_task::operator()()
{
	std::list<path> warnings;

	std::set<path> current_files;
	for(directory_iterator iter=directory_iterator(from_);
		iter!=directory_iterator(); ++iter)
	{
		const directory_entry &dent = *iter;
		if (dent.status().type()==directory_file)
		{
			//Recurse into subdir
			sync_task_ptr subtask(
						new upload_task(agenda_,  dent.path(), to_,
										cur_remote_+"/"+dent.filename()));
			agenda_->schedule(subtask);
		} else if (dent.status().type()==regular_file)
		{
			//Regular file
			current_files.insert(dent.path());
		} else if (dent.status().type()==symlink_file)
		{
			//Symlink
			//TODO: symlinks?
		} else
			warnings.push_back(dent.path());
	}

	//Retrieve the list of remote files
	s3_connection conn(to_, "GET", cur_remote_);
	std::cout << conn.read_fully() << std::endl;

	return result_code_t();
}

std::string upload_task::describe() const
{
	return "Scanning "+from_.file_string();
}

