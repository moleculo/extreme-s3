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

namespace es3
{
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
}; //namespace es3

static void check_entry(local_dir_ptr parent,
				   const bf::directory_entry &dent, synchronizer *sync)
{
	if (dent.path().string().find(" ")!=std::string::npos)
		return;

	if (dent.status().type()==bf::directory_file &&
			dent.symlink_status().type()!=bf::symlink_file)
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

		if (dent.status().type()==bf::regular_file &&
				dent.symlink_status().type()!=bf::symlink_file)
		{
			file->unsyncable_ = false;
		} else if (dent.symlink_status().type()==bf::symlink_file)
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
									 synchronizer *sync, bool upload)
{
	local_dir_ptr res(new local_dir());
	bf::path start(bf::absolute(start_path));
	if (start.filename()==".")
		start=start.parent_path();

	if (*start_path.rbegin() != '/' && upload)
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
						   std::vector<s3_path> remote,stringvec local,
						   bool do_upload, bool delete_missing,
						   const stringvec &included, const stringvec &excluded)
	: agenda_(agenda), ctx_(ctx), remote_(remote), local_(local),
	  do_upload_(do_upload), delete_missing_(delete_missing),
	  included_(included), excluded_(excluded)
{
}

bool synchronizer::check_included(const std::string &name)
{
	if (name.find(" ")!=std::string::npos)
		return false;

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

class list_subdir_task : public sync_task,
		public boost::enable_shared_from_this<list_subdir_task>
{
	s3_directory_ptr dir_;
	context_ptr ctx_;
public:
	list_subdir_task(s3_directory_ptr dir, context_ptr ctx) :
		dir_(dir), ctx_(ctx) {}

	virtual void print_to(std::ostream &str)
	{
		str << "Read dir " << dir_;
	}

	virtual task_type_e get_class() const { return taskUnbound; }

	virtual void operator()(agenda_ptr agenda)
	{
		s3_connection conn(ctx_);
		conn.list_files_shallow(dir_->absolute_name_, dir_, false);

		for(auto iter=dir_->subdirs_.begin();
			iter!=dir_->subdirs_.end();++iter)
		{
			agenda->schedule(sync_task_ptr(
								  new list_subdir_task(iter->second, ctx_)));
		}
	}
};

bool synchronizer::create_schedule(bool check_mode, bool delete_mode, 
								   bool non_recursive_delete)
{
	//Retrieve the list of remote files
	s3_connection conn(ctx_);

	local_dir_ptr locals;
	for(auto iter=local_.begin();iter!=local_.end();++iter)
	{
		assert(!delete_mode);
		local_dir_ptr cur(build_local_dir(*iter, this, do_upload_));
		if (locals)
			merge_to_left<local_dir_ptr, local_file_ptr>(locals, cur);
		else
			locals=cur;
	}

	VLOG(1)<<"Preparing S3 file list.";
	std::vector<s3_directory_ptr> remote_lists;
	for(auto iter=remote_.begin();iter!=remote_.end();++iter)
	{
		s3_directory_ptr cur_root=conn.list_files_shallow(
				*iter, s3_directory_ptr(), !do_upload_ || delete_mode);

		for(auto iter=cur_root->subdirs_.begin();
			iter!=cur_root->subdirs_.end();++iter)
		{
			agenda_->schedule(sync_task_ptr(
								  new list_subdir_task(iter->second, ctx_)));
		}
		remote_lists.push_back(cur_root);
	}
	agenda_->run();
	VLOG(1)<<"Preparing file list - done.";

	ctx_->reset(); //Reset CURLs	
	if (delete_mode)
	{
		for(auto iter=remote_lists.begin();iter!=remote_lists.end();++iter)
			delete_possibly_recursive(*iter, non_recursive_delete);
		return true;
	}
	
	s3_directory_ptr remotes;
	for(auto iter=remote_lists.begin();iter!=remote_lists.end();++iter)
	{
		if (remotes)
			merge_to_left<s3_directory_ptr,s3_file_ptr>(remotes, *iter);
		else
			remotes=*iter;
	}

	if (do_upload_)
	{
		process_upload(locals, remotes, remotes->absolute_name_, check_mode);
		return true;
	} else
	{
		process_downloads(remotes, locals, locals->absolute_name_, check_mode);
		return !remotes->files_.empty() || !remotes->subdirs_.empty();
	}
}

void synchronizer::delete_possibly_recursive(s3_directory_ptr dir, 
											 bool non_recursive)
{
	for(auto iter=dir->files_.begin();iter!=dir->files_.end();++iter)
	{
		if (!check_included(iter->second->absolute_name_.path_))
			continue;		
		sync_task_ptr task
				(new remote_file_deleter(ctx_, iter->second->absolute_name_));
		agenda_->schedule(task);
	}
	if (!non_recursive)
	{
		for(auto iter=dir->subdirs_.begin();iter!=dir->subdirs_.end();++iter)
			delete_possibly_recursive(iter->second, false);
	}
}

void synchronizer::process_upload(local_dir_ptr locals,
								  s3_directory_ptr remotes,
								  const s3_path &remote_path, bool check_mode)
{
	std::map<std::string, s3_file_ptr> unseen;
	std::map<std::string, s3_directory_ptr> unseen_dirs;
	if (remotes)
	{
		unseen.insert(remotes->files_.begin(), remotes->files_.end());
		unseen_dirs.insert(remotes->subdirs_.begin(), remotes->subdirs_.end());
	}

	for(auto iter=locals->files_.begin(); iter!=locals->files_.end();++iter)
	{
		local_file_ptr file = iter->second;
		unseen.erase(file->name_);
		unseen_dirs.erase(file->name_);
		if (file->unsyncable_) //Skip bad files
			continue;
		if (!check_included(file->absolute_name_.string()))
			continue;

		s3_path cur_remote_path = derive(remote_path, file->name_);
		if (remotes && remotes->subdirs_.count(file->name_))
		{
			if (delete_missing_)
			{
				delete_possibly_recursive(remotes->subdirs_[file->name_], false);
				sync_task_ptr task(new file_uploader(
					ctx_, file->absolute_name_, cur_remote_path));
				agenda_->schedule(task);
			} else
			{
				VLOG(0) << "Local file "<< file->absolute_name_ << " "
						<< "shadows directory on the remote side, but we're "
						<< "not allowed to remove it.";
			}
		} else
		{
			if (!check_mode || !remotes->files_.count(file->name_))
			{
				sync_task_ptr task(new file_uploader(
					ctx_, file->absolute_name_, cur_remote_path));
				agenda_->schedule(task);
			}
		}
	}

	for(auto iter=locals->subdirs_.begin(); iter!=locals->subdirs_.end();++iter)
	{
		local_dir_ptr dir = iter->second;
		unseen.erase(dir->name_);
		unseen_dirs.erase(dir->name_);
		s3_path cur_remote_path = derive(remote_path, dir->name_);

		if (remotes && remotes->files_.count(dir->name_))
		{
			if (delete_missing_)
			{
				sync_task_ptr task(new remote_file_deleter(ctx_,
					remotes->files_[dir->name_]->absolute_name_));
				agenda_->schedule(task);
				process_upload(dir, s3_directory_ptr(), cur_remote_path, check_mode);
			} else
			{
				VLOG(0) << "Local dir "<< dir->absolute_name_ << " "
						<< "is shadowed by file on the remote side, but we're "
						<< "not allowed to remove it.";
			}
		} else
		{
			process_upload(dir, remotes?try_get(remotes->subdirs_, dir->name_):
					s3_directory_ptr(), cur_remote_path, check_mode);
		}
	}

	if (delete_missing_)
	{
		for(auto iter=unseen.begin();iter!=unseen.end();++iter)
		{
			sync_task_ptr task(new remote_file_deleter(ctx_,
				iter->second->absolute_name_));
			agenda_->schedule(task);
		}
		for(auto iter=unseen_dirs.begin();iter!=unseen_dirs.end();++iter)
			delete_possibly_recursive(iter->second, false);
	}
}

void synchronizer::process_downloads(s3_directory_ptr remotes,
	local_dir_ptr locals, const bf::path &local_path, bool check_mode)
{
	std::map<std::string, bf::path> unseen;
	if (locals)
	{
		for(auto f=locals->files_.begin();f!=locals->files_.end();++f)
			unseen[f->first]=f->second->absolute_name_;
		for(auto f=locals->subdirs_.begin();f!=locals->subdirs_.end();++f)
			unseen[f->first]=f->second->absolute_name_;
	}

	for(auto iter=remotes->files_.begin(); iter!=remotes->files_.end();++iter)
	{
		s3_file_ptr file = iter->second;
		unseen.erase(file->name_);
		if (!check_included(file->absolute_name_.path_))
			continue;

		bf::path cur_local_path = local_path / file->name_;
		bool shadowed=locals && locals->subdirs_.count(file->name_);
		if (shadowed && !delete_missing_)
		{
			VLOG(0) << "Remote file "<< file->absolute_name_ << " "
					<< "is shadowed by a local directory, "
					<< "but we're not allowed to remove it.";
		} else
		{
			if (!check_mode || !locals->files_.count(file->name_))
			{
				sync_task_ptr task(new file_downloader(
					ctx_, cur_local_path, file->absolute_name_, shadowed));
				agenda_->schedule(task);
			}
		}
	}

	for(auto iter=remotes->subdirs_.begin(); iter!=remotes->subdirs_.end();++iter)
	{
		s3_directory_ptr dir = iter->second;
		unseen.erase(dir->name_);

		bf::path cur_local_path = local_path / dir->name_;

		local_dir_ptr new_dir;
		if (locals)
			new_dir=try_get(locals->subdirs_, dir->name_);

		bool shadowed=locals && locals->files_.count(dir->name_) ;
		if (shadowed && !delete_missing_)
		{
			VLOG(0) << "Remote dir "<< dir->absolute_name_ << " "
					<< "is shadowed by a local file, but we're "
					<< "not allowed to remove it.";
		} else
		{
			if (shadowed)
				local_file_deleter(cur_local_path)(agenda_ptr());

			if (!new_dir)
			{
				int res=mkdir(cur_local_path.c_str(), 0755);
				if (res && errno!=EEXIST)
					res | libc_die2("Failed to create "+cur_local_path.string());
			}
			process_downloads(dir, new_dir, cur_local_path, check_mode);
		}
	}

	if (delete_missing_)
	{
		for(auto iter=unseen.begin();iter!=unseen.end();++iter)
			agenda_->schedule(sync_task_ptr(
								  new local_file_deleter(iter->second)));
	}
}
