#include "commands.h"
#include "connection.h"
#include "sync.h"
#include <iostream>
#include <boost/program_options.hpp>

using namespace es3;
namespace po = boost::program_options;

int es3::do_rsync(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help)
{
	po::options_description opts("Sync options", term_width);
	stringvec included, excluded;
	opts.add_options()
		("delete-missing,D", "Delete missing files from the sync destination")
		("exclude-path,E", po::value<stringvec>(&excluded),
			"Exclude the paths matching the pattern from synchronization. "
			"If set, all matching files will be excluded even if they match "
			"one of the 'include-path' rules.")
		("include-path,I", po::value<stringvec>(&included),
			"Include the paths matching the pattern for synchronization. "
			"If set, only the matching paths will be included in "
			"synchronization.")
	;

	if (help)
	{
		std::cout << "Sync syntax: es3 sync [OPTIONS] <SOURCES> <DESTINATION>\n"
				  << "where <SOURCES> and <DESTINATION> are either:\n"
				  << "\t - Local directory\n"
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		std::cout << opts;
		return 0;
	}

	po::positional_options_description pos;
	pos.add("<ARGS>", -1);

	stringvec args;
	opts.add_options()
			("<ARGS>", po::value<stringvec>(&args)->multitoken()->required())
	;

	po::variables_map vm;
	try
	{
		po::store(po::command_line_parser(params)
			.options(opts).positional(pos).run(), vm);
		po::notify(vm);
	} catch(const boost::program_options::error &err)
	{
		std::cerr << "ERR: Failed to parse configuration options. Error: "
				  << err.what() << "\n"
				  << "Use --help for help\n";
		return 2;
	}
	if (args.size()<2)
	{
		std::cerr << "ERR: At least one <SOURCE> "
				  << "and exactly one <DESTINATION> must be specified.\n";
		return 2;
	}

	bool delete_missing=vm.count("delete-missing");

	s3_connection conn(context);
	std::vector<s3_path> remotes;
	stringvec locals;
	bool do_upload;

	std::string tgt = args.back();
	args.pop_back();
	if (tgt.find("s3://")==0)
	{
		//Upload!
		s3_path path = parse_path(tgt);
		std::string region=conn.find_region(path.bucket_);
		path.zone_=region;
		remotes.push_back(path);

		//Check that local sources exist
		for(auto iter=args.begin();iter!=args.end();++iter)
		{
			if (!bf::exists(*iter))
			{
				std::cerr << "ERR: Non-existing path " << *iter << std::endl;
				return 3;
			}
		}
		locals = args;
		do_upload=true;
	} else
	{
		locals.push_back(tgt);
		if (!bf::exists(tgt))
		{
			bf::create_directories(tgt);
			if (!bf::exists(tgt))
			{
				std::cerr << "ERR: Non-existing path " << tgt << std::endl;
				return 3;
			}
		}
		for(auto iter=args.begin();iter!=args.end();++iter)
		{
			s3_path path = parse_path(*iter);
			std::string region=conn.find_region(path.bucket_);
			path.zone_=region;
			remotes.push_back(path);
		}

		do_upload = false;
	}

	for(int f=0;f<3;++f)
	{
		synchronizer sync(ag, context, remotes, locals, do_upload,
						  delete_missing, included, excluded);
		if (!sync.create_schedule(false, false, false))
		{
			std::cerr << "ERR: <SOURCE> not found.\n";
			return 2;
		}

		int res=ag->run();
		if (res!=0)
			return res;
		if (!ag->tasks_count())
		{
			ag->print_epilog();
			return 0;
		}
		//We still have pending tasks. Try once again.
	}

	if (ag->tasks_count())
	{
		ag->print_epilog(); //Print stats, so they're at least visible
		std::cerr << "ERR: ";
		ag->print_queue();
		return 4;
	}

	return 0;
}

int es3::do_test(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help)
{
	if (help)
	{
		std::cout << "Test syntax: es3 test <PATH>\n"
				  << "where <PATH> is either:\n"
				  << "\t - Local file/directory\n"
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		return 0;
	}
	if (params.size()!=1)
	{
		std::cerr << "ERR: <PATH> must be specified.\n";
		return 2;
	}

	std::string tgt = params.at(0);
	if (tgt.find("s3://")==0)
	{
		s3_path path = parse_path(tgt);
		s3_connection conn(context);
		std::string region=conn.find_region(path.bucket_);
		path.zone_=region;

		s3_directory_ptr ptr=conn.list_files_shallow(path,
													 s3_directory_ptr(),  true);
		if (!ptr->subdirs_.empty() || !ptr->files_.empty())
			return 0;
		else
			return 1;
	} else
	{
		if (bf::exists(bf::path(tgt)))
			return 0;
		else
			return 1;
	}
}

int es3::do_touch(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help)
{
	if (help)
	{
		std::cout << "Touch syntax: es3 test <PATH>\n"
				  << "where <PATH> is either:\n"
				  << "\t - Local file/directory\n"
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		return 0;
	}
	if (params.size()!=1)
	{
		std::cerr << "ERR: <PATH> must be specified.\n";
		return 2;
	}

	std::string tgt = params.at(0);
	if (tgt.find("s3://")==0)
	{
		s3_path path = parse_path(tgt);
		s3_connection conn(context);
		std::string region=conn.find_region(path.bucket_);
		path.zone_=region;

		s3_directory_ptr ptr=conn.list_files_shallow(path,
													 s3_directory_ptr(), true);
		if (ptr->subdirs_.empty() && ptr->files_.empty())
			conn.upload_data(path, "", 0);
		return 0;
	} else
	{
		return system(("touch "+tgt).c_str());
	}
}

int es3::do_rm(context_ptr context, const stringvec& params,
		 agenda_ptr ag, bool help)
{
	po::options_description opts("rm options", term_width);
	stringvec included, excluded;
	opts.add_options()
		("recursive,r", "Delete recursively")
		("exclude-path,E", po::value<stringvec>(&excluded),
			"Exclude the paths matching the pattern from deletion. "
			"If set, all matching files will be excluded even if they match "
			"one of the 'include-path' rules.")
		("include-path,I", po::value<stringvec>(&included),
			"Include the paths matching the pattern for deletion. "
			"If set, only the matching paths will be deleted")
	;

	if (help)
	{
		std::cout << "rm syntax: es3 rm [OPTIONS] <PATH>\n"
				  << "where <PATH> is:\n"
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		std::cout << opts;
		return 0;
	}

	po::positional_options_description pos;
	pos.add("<ARGS>", -1);

	stringvec args;
	opts.add_options()
			("<ARGS>", po::value<stringvec>(&args)->multitoken()->required())
	;
	
	po::variables_map vm;
	try
	{
		po::store(po::command_line_parser(params)
			.options(opts).positional(pos).run(), vm);
		po::notify(vm);
	} catch(const boost::program_options::error &err)
	{
		std::cerr << "ERR: Failed to parse configuration options. Error: "
				  << err.what() << "\n"
				  << "Use --help for help\n";
		return 2;
	}
	if (args.size()<1)
	{
		std::cerr << "ERR: At least one <PATH> must be specified.\n";
		return 2;
	}

	bool recursive=vm.count("recursive");
	
	s3_connection conn(context);
	std::vector<s3_path> remotes;
	stringvec locals;

	for(auto iter=args.begin();iter!=args.end();++iter)
	{
		s3_path path = parse_path(*iter);
		std::string region=conn.find_region(path.bucket_);
		path.zone_=region;
		remotes.push_back(path);
	}

	//We're abusing the synchronizer here by asking it to synchronize with
	//an empty source path (remote or local) and delete missing files.
	//The end result is fast and efficient RM.
	for(int f=0;f<3;++f)
	{
		synchronizer sync(ag, context, remotes, locals, true, true, 
						  included, excluded);
		if (!sync.create_schedule(false, true, !recursive))
		{
			std::cerr << "ERR: <PATH> not found.\n";
			return 2;
		}

		int res=ag->run();
		if (res!=0)
			return res;
		if (!ag->tasks_count())
		{
			ag->print_epilog();
			return 0;
		}
		//We still have pending tasks. Try once again.
	}

	if (ag->tasks_count())
	{
		ag->print_epilog(); //Print stats, so they're at least visible
		std::cerr << "ERR: ";
		ag->print_queue();
		return 4;
	}

	return 0;
	
}

struct stat_struct
{
	uint64_t size_, file_num_, dir_num_;
	uint64_t recent_timestamp_;
};

static void get_size(s3_directory_ptr cur, stat_struct *out)
{
	for(auto iter=cur->files_.begin(); iter!=cur->files_.end();++iter)
	{
		out->size_+=iter->second->size_;
		out->file_num_++;
		const std::string &mtime=iter->second->mtime_str_;
		
		uint64_t tm=boost::lexical_cast<uint64_t>(mtime.c_str());
		if (tm>out->recent_timestamp_)
			out->recent_timestamp_=tm;
	}
	
	for(auto iter=cur->subdirs_.begin(); iter!=cur->subdirs_.end();++iter)
	{
		out->dir_num_++;
		get_size(iter->second, out);
	}
}

int es3::do_du(context_ptr context, const stringvec& params,
		 agenda_ptr ag, bool help)
{
	if (help)
	{
		std::cout << "Test syntax: es3 du <PATH>\n"
				  << "where <PATH> is:\n"					 
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		return 0;
	}
	if (params.size()!=1)
	{
		std::cerr << "ERR: <PATH> must be specified.\n";
		return 2;
	}
	
	std::string tgt = params.at(0);		
	s3_connection conn(context);

	s3_path path = parse_path(tgt);
	std::string region=conn.find_region(path.bucket_);
	path.zone_=region;

	//Do recursive ls
	s3_directory_ptr cur_root=schedule_recursive_walk(path, context, ag);
	
	int res=ag->run();
	if (res!=0)
		return res;
	
	if (ag->tasks_count())
	{
		ag->print_epilog(); //Print stats, so they're at least visible
		std::cerr << "ERR: ";
		ag->print_queue();
		return 4;
	}
	
	//Calculate total size and print it
	stat_struct st={0};
	get_size(cur_root, &st);
	
	std::cout<<"Total files: " << st.file_num_ << std::endl;
	std::cout<<"Total directories: " << st.dir_num_ << std::endl;
	std::cout<<"Total size: " << st.size_ << std::endl;
	std::cout<<"Most recent timestamp: " << st.recent_timestamp_ << std::endl;

	return 0;
}

class get_file_info: public sync_task,
		public boost::enable_shared_from_this<get_file_info>
{
	const s3_path path_;
	context_ptr context_;
	std::map<s3_path, file_desc> &desc_map_;
	std::mutex &mtx_;
public:
	get_file_info(const s3_path &path, context_ptr context,
				  std::map<s3_path, file_desc> &desc, std::mutex &mtx) :
		path_(path), context_(context), desc_map_(desc), mtx_(mtx)
	{
	}

	virtual task_type_e get_class() const { return taskUnbound; }

	virtual void print_to(std::ostream &str)
	{
		str << "Get info about " << path_;
	}

	virtual void operator()(agenda_ptr agenda)
	{
		s3_connection conn(context_);
		file_desc mod=conn.find_mtime_and_size(path_);
		
		guard_t lock(mtx_);
		desc_map_[path_] = mod;
	}
};

int es3::do_ls(context_ptr context, const stringvec& params,
		 agenda_ptr ag, bool help)
{
	if (help)
	{
		std::cout << "Test syntax: es3 ls <PATH>\n"
				  << "where <PATH> is:\n"					 
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		return 0;
	}
	if (params.size()!=1)
	{
		std::cerr << "ERR: <PATH> must be specified.\n";
		return 2;
	}
	
	std::string tgt = params.at(0);		
	s3_connection conn(context);

	s3_path path = parse_path(tgt);
	std::string region=conn.find_region(path.bucket_);
	path.zone_=region;

	//Do non-recursive ls
	s3_directory_ptr cur=conn.list_files_shallow(
				path, s3_directory_ptr(), true);
	size_t files=0, dirs=0;
	uint64_t total=0;
	for(auto iter=cur->subdirs_.begin(); iter!=cur->subdirs_.end();++iter)
	{
		std::cout << "\t\tDIR\t" << iter->second->absolute_name_ << std::endl;
		dirs++;
	}
	
	if (cur->files_.size()>10)
	{
		std::map<s3_path, file_desc> desc_map;
		std::mutex desc_mtx;	
		for(auto iter=cur->files_.begin(); iter!=cur->files_.end();++iter)
		{
			sync_task_ptr tsk(new get_file_info(iter->second->absolute_name_,
												context, desc_map, desc_mtx));
			ag->schedule(tsk);
		}
		int res=ag->run();
		if (res!=0)
			return res;
		
		if (ag->tasks_count())
		{
			ag->print_epilog(); //Print stats, so they're at least visible
			std::cerr << "ERR: ";
			ag->print_queue();
			return 4;
		}
		
		for(auto iter=cur->files_.begin(); iter!=cur->files_.end();++iter)
		{
			s3_path remote_name = iter->second->absolute_name_;
			const file_desc &mod=desc_map.at(remote_name);
			std::cout << mod.mtime_
					  << "\t"<< mod.raw_size_
					  << "\t" << remote_name << std::endl;
			files++;
			total+=iter->second->size_;
		}
	} else
	{
		for(auto iter=cur->files_.begin(); iter!=cur->files_.end();++iter)
		{
			s3_path remote_name = iter->second->absolute_name_;
			file_desc mod=conn.find_mtime_and_size(remote_name);
			std::cout << mod.mtime_
					  << "\t"<< mod.raw_size_
					  << "\t" << remote_name << std::endl;
			files++;
			total+=iter->second->size_;
		}		
	}
	
	std::cout<<"Total files: " << files << std::endl;
	std::cout<<"Total directories: " << dirs << std::endl;
	std::cout<<"Total size: " << total << std::endl;

	return 0;
}

int es3::do_publish(context_ptr context, const stringvec& params,
		 agenda_ptr ag, bool help)
{
	po::options_description opts("publish options", term_width);
	stringvec included, excluded;
	opts.add_options()
		("exclude-path,E", po::value<stringvec>(&excluded),
			"Exclude the paths matching the pattern from deletion. "
			"If set, all matching files will be excluded even if they match "
			"one of the 'include-path' rules.")
		("include-path,I", po::value<stringvec>(&included),
			"Include the paths matching the pattern for deletion. "
			"If set, only the matching paths will be deleted")
	;

	if (help)
	{
		std::cout << "rm syntax: es3 publish [OPTIONS] <PATH>\n"
				  << "where <PATH> is:\n"
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		std::cout << opts;
		return 0;
	}

	po::positional_options_description pos;
	pos.add("<ARGS>", -1);
	stringvec args;
	opts.add_options()
			("<ARGS>", po::value<stringvec>(&args)->multitoken()->required())
	;	
	po::variables_map vm;
	try
	{
		po::store(po::command_line_parser(params)
			.options(opts).positional(pos).run(), vm);
		po::notify(vm);
	} catch(const boost::program_options::error &err)
	{
		std::cerr << "ERR: Failed to parse configuration options. Error: "
				  << err.what() << "\n"
				  << "Use --help for help\n";
		return 2;
	}
	if (args.size()<1)
	{
		std::cerr << "ERR: At least one <PATH> must be specified.\n";
		return 2;
	}
	
	s3_connection conn(context);

	//Do recursive publication
	size_t num=0;
	for(auto iter=args.begin();iter!=args.end();++iter)
	{
		s3_path path = parse_path(*iter);
		path.zone_=conn.find_region(path.bucket_);
		schedule_recursive_publication(path, context, ag, included, excluded, &num);
	}
	
	int res=ag->run();
	if (res!=0)
		return res;
	
	if (ag->tasks_count())
	{
		ag->print_epilog(); //Print stats, so they're at least visible
		std::cerr << "ERR: ";
		ag->print_queue();
		return 4;
	}
	
	std::cout<<"Total files published: " << num << std::endl;
	return 0;	
}
