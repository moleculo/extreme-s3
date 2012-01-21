#include "sync.h"
#include "uploader.h"
#include "downloader.h"
#include "context.h"
#include "errors.h"
#include <set>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include "pattern_match.hpp"

using namespace es3;

struct local_file;
struct local_dir;
typedef boost::shared_ptr<local_file> local_file_ptr;
typedef boost::shared_ptr<local_dir> local_dir_ptr;

struct local_file
{
	bf::path absolute_name_;
	std::string name_;
	bool unsyncable_;
};

struct local_dir
{
	bf::path absolute_name_;
	std::string name_;

	std::map<std::string, local_file_ptr> files_;
	std::map<std::string, local_dir_ptr> subdirs_;
};

static void check_entry(local_dir_ptr parent,
				   const bf::directory_entry &dent, synchronizer *sync)
{
	if (!sync->check_included(dent.path().string()))
		return;

	if (dent.status().type()==bf::directory_file)
	{
		//Recurse into subdir
		local_dir_ptr target(new local_dir());
		target->absolute_name_ = bf::absolute(dent.path());
		target->name_ = dent.path().filename().string();
		parent->subdirs_[target->name_]=target;

		for(bf::directory_iterator iter=bf::directory_iterator(dent.path());
			iter!=bf::directory_iterator(); ++iter)
		{
			check_entry(target, *iter, sync);
		}
	} else
	{
		local_file_ptr file(new local_file());
		file->absolute_name_=bf::absolute(dent.path());
		file->name_ = file->absolute_name_.filename().string();
		parent->files_[file->name_] = file;

		if (dent.status().type()==bf::regular_file)
		{
			file->unsyncable_ = false;
		} else if (dent.status().type()==bf::symlink_file)
		{
			file->unsyncable_ = true;
			VLOG(2) << "Symlink skipped "<< dent.path();
		} else
		{
			file->unsyncable_ = true;
			VLOG(1) << "Unknown local file type "<< dent.path();
		}
	}
}

static local_dir_ptr build_local_dir(const std::string &start_path,
									 synchronizer *sync)
{
	local_dir_ptr res(new local_dir());
	bf::path start(bf::absolute(start_path));

	if (*start_path.rbegin() != '/')
	{
		//A tricky bit - we're actually synchronizing path/../path, not path/*
		res->absolute_name_ = start.parent_path();
		res->name_ = res->absolute_name_.filename().string();
		check_entry(res, bf::directory_entry(start, bf::status(start)),
					sync);
	} else
	{
		res->absolute_name_ = start;
		res->name_ = res->absolute_name_.filename().string();
		for(bf::directory_iterator iter=bf::directory_iterator(start);
			iter!=bf::directory_iterator(); ++iter)
		{
			check_entry(res, *iter, sync);
		}
	}

	return res;
}

template<class dir_ptr_t, class file_ptr_t>
	static void merge_to_left(dir_ptr_t left, dir_ptr_t right)
{
	//Merge files
	for(auto iter=right->files_.begin(), iend=right->files_.end();
		iter!=iend; ++iter)
	{
		file_ptr_t right_file = iter->second;
		if (left->files_.count(right_file->name_))
		{
			err(errFatal) << "File name collision: "
						  << right_file->absolute_name_
						  << " collides with  "
						  << left->files_[right_file->name_]->absolute_name_;
		}

		if (left->subdirs_.count(right_file->name_))
		{
			//Uh-oh.
			err(errFatal) << "File "
						  << right_file->absolute_name_
						  << " shadows directory "
						  << left->subdirs_[right_file->name_]->absolute_name_;
		}

		left->files_[right_file->name_] = right_file;
	}

	//Merge directories
	for(auto iter=right->subdirs_.begin(), iend=right->subdirs_.end();
		iter!=iend; ++iter)
	{
		dir_ptr_t right_dir= iter->second;

		if (left->files_.count(right_dir->name_))
		{
			//Uh-oh.
			err(errFatal) << "Directory "
						  << right_dir->absolute_name_
						  << " is shadowed by "
						  << left->files_[right_dir->name_]->absolute_name_;
		}

		if (left->subdirs_.count(right_dir->name_))
		{
			merge_to_left<dir_ptr_t, file_ptr_t>(
						left->subdirs_[right_dir->name_], right_dir);
		} else
			left->subdirs_[right_dir->name_] = right_dir;
	}
}

synchronizer::synchronizer(agenda_ptr agenda, const context_ptr &ctx,
						   stringvec remote,stringvec local,
						   bool do_upload, bool delete_missing,
						   const stringvec &included, const stringvec &excluded)
	: agenda_(agenda), ctx_(ctx), remote_(remote), local_(local),
	  do_upload_(do_upload), delete_missing_(delete_missing),
	  included_(included), excluded_(excluded)
{
}

bool synchronizer::check_included(const std::string &name)
{
	if (!excluded_.empty())
	{
		for(auto f = excluded_.begin();f!=excluded_.end();++f)
			if (pattern_match(*f)(name))
				return false;
	}
	if (!included_.empty())
	{
		for(auto f = included_.begin();f!=included_.end();++f)
			if (pattern_match(*f)(name))
				return true;
		return false;
	}
	return true;
}

void synchronizer::create_schedule()
{
	//Retrieve the list of remote files
	s3_connection conn(ctx_);

	local_dir_ptr locals;
	for(auto iter=local_.begin();iter!=local_.end();++iter)
	{
		local_dir_ptr cur(build_local_dir(*iter, this));
		if (locals)
			merge_to_left<local_dir_ptr, local_file_ptr>(locals, cur);
		else
			locals=cur;
	}

	s3_directory_ptr remotes;
	for(auto iter=remote_.begin();iter!=remote_.end();++iter)
	{
		s3_directory_ptr cur = conn.list_files(*iter);
		if (remotes)
			merge_to_left<s3_directory_ptr,s3_file_ptr>(remotes, cur);
		else
			remotes=cur;
	}

//	std::string prefix = remote_;
//	if (!prefix.empty() && prefix.at(0)=='/')
//		prefix=prefix.substr(1);
//	file_map_t remotes = conn.list_files(prefix);
//	process_dir(&remotes, remote_, local_);
}

/*
void synchronizer::create_schedule()
{
	//Retrieve the list of remote files
	s3_connection conn(ctx_);

	std::string prefix = remote_;
	if (!prefix.empty() && prefix.at(0)=='/')
		prefix=prefix.substr(1);
	file_map_t remotes = conn.list_files(prefix);
	process_dir(&remotes, remote_, local_);
}

void synchronizer::process_dir(file_map_t *remote_list,
	const std::string &remote_dir, const bf::path &local_dir)
{
	assert(remote_dir.at(remote_dir.size()-1)=='/');
	if(bf::status(local_dir).type()!=bf::directory_file)
		err(errFatal) << "Directory "<<local_dir<<" can't be accessed";

	file_map_t cur_remote_copy = remote_list ?
				*remote_list : file_map_t();

	for(bf::directory_iterator iter=bf::directory_iterator(local_dir);
		iter!=bf::directory_iterator(); ++iter)
	{
		const bf::directory_entry &dent = *iter;
		bf::path cur_local_path=bf::absolute(dent.path());
		std::string cur_local_filename=cur_local_path.filename().string();
		std::string cur_remote_path = remote_dir+cur_local_filename;

		remote_file_ptr cur_remote_child=try_get(
					cur_remote_copy, cur_local_filename, remote_file_ptr());
		cur_remote_copy.erase(cur_local_filename);

		if (dent.status().type()==bf::directory_file)
		{
			//Recurse into subdir
			if (cur_remote_child)
			{
				if (!cur_remote_child->is_dir_)
				{
					if (delete_missing_)
					{
						if (do_upload_)
						{
							sync_task_ptr task(new remote_file_deleter(ctx_,
								 cur_remote_child->full_name_));
							agenda_->schedule(task);
							process_dir(0, cur_remote_path+"/", cur_local_path);
						} else
						{
							sync_task_ptr task(new file_downloader(ctx_,
								cur_local_path, cur_remote_path, true));
							if (check_included(cur_local_filename))
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
						cur_remote_path+"/", cur_local_path);
			} else
			{
				if (do_upload_)
				{
					process_dir(0, cur_remote_path+"/", cur_local_path);
				} else
				{
					if (delete_missing_)
					{
						sync_task_ptr task(new local_file_deleter(cur_local_path));
						if (check_included(cur_local_filename))
							agenda_->schedule(task);
					}
				}
			}
		} else if (dent.status().type()==bf::regular_file)
		{
			//Regular file
			if (do_upload_)
			{
				sync_task_ptr task(new file_uploader(
					ctx_, cur_local_path, cur_remote_path));
				if (check_included(cur_local_filename))
					agenda_->schedule(task);
			} else
			{
				if (!cur_remote_child)
				{
					if (delete_missing_)
					{
						sync_task_ptr task(new local_file_deleter(cur_local_path));
						if (check_included(cur_local_filename))
							agenda_->schedule(task);
					}
				} else
				{
					sync_task_ptr task(new file_downloader(
						ctx_, cur_local_path, cur_remote_path));
					if (check_included(cur_local_filename))
						agenda_->schedule(task);
				}
			}
		} else
		{
			VLOG(1) << "Unknown local file type "<< cur_local_path.string();
		}
	}

	if (delete_missing_ || !do_upload_)
		process_missing(cur_remote_copy, local_dir);
}

void synchronizer::process_missing(const file_map_t &cur,
								   const bf::path &cur_local_dir)
{
	for(auto f = cur.begin(); f!=cur.end();++f)
	{
		std::string filename = f->first;
		remote_file_ptr file = f->second;

		if (file->is_dir_)
		{
			bf::path new_dir = cur_local_dir / filename;
			if (!do_upload_)
				mkdir(new_dir.c_str(), 0755) |
					libc_die2("Failed to create "+new_dir.string());
			process_missing(file->children_, new_dir);
		} else
		{
			if (!check_included(filename))
				continue;

			if (do_upload_)
			{
				if (delete_missing_)
				{
					agenda_->schedule(sync_task_ptr(
						new remote_file_deleter(ctx_, file->full_name_)));
				}
			} else
			{
				//Missing local file just means that we need to download it
				agenda_->schedule(sync_task_ptr(
					new file_downloader(ctx_, cur_local_dir / filename,
										file->full_name_)));
			}
		}
	}
}
*/
