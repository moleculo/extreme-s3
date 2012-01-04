#include "sync.h"
#include "uploader.h"
#include "downloader.h"
#include "context.h"
#include <set>
#include <boost/filesystem.hpp>
#include <iostream>
#include "workaround.hpp"

using namespace es3;
using namespace boost::filesystem;

synchronizer::synchronizer(agenda_ptr agenda, const context_ptr &to)
	: agenda_(agenda), to_(to)
{
}

void synchronizer::create_schedule()
{
	//Retrieve the list of remote files
	s3_connection conn(to_);
	file_map_t remotes = conn.list_files(to_->remote_root_, "");
	process_dir(&remotes, to_->local_root_, to_->remote_root_);
}

void synchronizer::process_dir(file_map_t *cur_remote,
							  const std::string &cur_local,
							  const std::string &cur_remote_path)
{
	file_map_t cur_remote_copy = cur_remote ? *cur_remote : file_map_t();

	for(directory_iterator iter=directory_iterator(cur_local);
		iter!=directory_iterator(); ++iter)
	{
		const directory_entry &dent = *iter;
		std::string new_remote_path = cur_remote_path+
				get_file(dent.path().filename());

		remote_file_ptr cur_remote_child=try_get(
					cur_remote_copy, get_file(dent.path().filename()),
					remote_file_ptr());
		cur_remote_copy.erase(get_file(dent.path().filename()));

		std::string dent_path=get_file(
					system_complete(dent.path()).string());

		if (dent.status().type()==directory_file)
		{
			//Recurse into subdir
			if (cur_remote_child)
			{
				if (!cur_remote_child->is_dir_)
				{
					if (to_->delete_missing_)
					{
						if (to_->upload_)
						{
							sync_task_ptr task(new file_deleter(to_,
								 cur_remote_child->full_name_));
							agenda_->schedule(task);
							process_dir(0, dent_path, new_remote_path+"/");
						} else
						{
							sync_task_ptr task(new file_downloader(to_,
								dent_path, new_remote_path, "", true));
							agenda_->schedule(task);
						}
					} else
					{
						VLOG(0) << "Local file "<< dent.path() << " "
								<< "has become a directory, but we're "
								<< "not allowed to remove it on the "
								<< "remote side";
					}
				} else
					process_dir(&cur_remote_child->children_,
						dent_path, new_remote_path+"/");
			} else
			{
				if (to_->upload_)
				{
					process_dir(0, dent_path, new_remote_path+"/");
				}
				else
				{
					sync_task_ptr task(new local_file_deleter(dent_path));
					agenda_->schedule(task);
				}
			}
		} else if (dent.status().type()==regular_file)
		{
			//Regular file
			if (to_->upload_)
			{
				if (!cur_remote_child)
				{
					sync_task_ptr task(new file_uploader(
						to_, dent_path, new_remote_path, ""));
					agenda_->schedule(task);
				}
				else
				{
					sync_task_ptr task(new file_uploader(
						to_, dent_path, new_remote_path,
						cur_remote_child->etag_));
					agenda_->schedule(task);
				}
			} else
			{
				if (!cur_remote_child)
				{
					sync_task_ptr task(new local_file_deleter(dent_path));
					agenda_->schedule(task);
				}
				else
				{
					sync_task_ptr task(new file_downloader(
						to_, dent_path, new_remote_path,
						cur_remote_child->etag_));
					agenda_->schedule(task);
				}
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

	if (to_->delete_missing_)
		process_missing(cur_remote_copy, cur_remote_path);
}

void synchronizer::process_missing(const file_map_t &cur,
								   const std::string &cur_local_path)
{
	for(auto f = cur.begin(); f!=cur.end();++f)
	{
		if (f->second->is_dir_)
			process_missing(f->second->children_,
							cur_local_path+f->first+"/");
		else
		{
			if (to_->upload_)
			{
				agenda_->schedule(sync_task_ptr(
					new file_deleter(to_, f->second->full_name_)));
			} else
			{
				//Missing local file just means that we need to download it
				agenda_->schedule(sync_task_ptr(
					new file_downloader(to_, cur_local_path+f->first,
										f->second->full_name_,"")));
			}
		}
	}
}
