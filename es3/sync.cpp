#include "sync.h"
#include <set>
#include <boost/filesystem.hpp>
#include <iostream>

using namespace es3;
using namespace boost::filesystem;

synchronizer::synchronizer(agenda_ptr agenda, const connection_data &to)
	: agenda_(agenda), to_(to)
{
}

void synchronizer::create_schedule()
{
	//Retrieve the list of remote files
	s3_connection conn(to_, "GET", to_.remote_root_);
	file_map_t remotes = conn.list_files("");
	process_dir(&remotes, to_.local_root_, to_.remote_root_);
}

void synchronizer::process_dir(file_map_t *cur_remote,
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
						VLOG(0) << "Local file "<< dent.path() << " "
								<< "has become a directory, but we're "
								<< "not allowed to remove it on the "
								<< "remote side";
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
				agenda_->schedule_upload(to_, dent.path(), new_remote_path,
										 cur_remote_child->etag_);
			}
		} else if (dent.status().type()==symlink_file)
		{
			//Symlink
			//TODO: symlinks?
		} else
		{
			VLOG(0) << "Unknown local file "<< dent.path();
		}
	}
}
