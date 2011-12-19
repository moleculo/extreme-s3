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
	//Retrieve the list of remote files
	s3_connection conn(to_, "GET", cur_remote_);
	file_map_t remotes = conn.list_files("");
	process_dir(&remotes, from_, "");

	return result_code_t();
}

void upload_task::process_dir(file_map_t *cur_remote,
							  const boost::filesystem::path &cur_local,
							  const std::string &cur_remote_path)
{
	for(directory_iterator iter=directory_iterator(cur_local);
		iter!=directory_iterator(); ++iter)
	{
		const directory_entry &dent = *iter;
		std::string new_remote_path = cur_remote_path+"/"+
				dent.path().filename().string();
		remote_file_ptr cur_remote_child;
		if (cur_remote)
			cur_remote_child=try_get(*cur_remote,
									 dent.path().filename().string(),
									 remote_file_ptr());

		if (dent.status().type()==directory_file)
		{
			//Recurse into subdir
			if (cur_remote_child)
			{
				if (!cur_remote_child->is_dir_)
				{
					if (to_.delete_missing_)
					{
						agenda_->schedule_removal(cur_remote_child);
						process_dir(0, dent.path(), new_remote_path);
					} else
					{
						VLOG_MACRO(1) << "File became a directory, but we're "
								   "not allowed to remove it";
					}
				} else
				{
					process_dir(&cur_remote_child->children_,
								dent.path(), new_remote_path);
				}
			} else
			{
				process_dir(0, dent.path(), new_remote_path);
			}
		} else if (dent.status().type()==regular_file)
		{
			//Regular file
			if (!cur_remote_child)
				agenda_->schedule_upload(to_, dent.path(), new_remote_path, "");
			else
			{
				uint64_t our_size=file_size(dent.path());
				if (our_size != cur_remote_child->size_)
					agenda_->schedule_upload(to_,
											 dent.path(), new_remote_path, "");
				else
					agenda_->schedule_upload(to_, dent.path(), new_remote_path,
											 cur_remote_child->etag_);
			}
		} else if (dent.status().type()==symlink_file)
		{
			//Symlink
			//TODO: symlinks?
		} else
			warnings_.push_back(dent.path());
	}
}

std::string upload_task::describe() const
{
	return "Scanning "+from_.string();
}
