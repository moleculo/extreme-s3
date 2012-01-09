#include <iostream>
#include <boost/program_options.hpp>
#include "common.h"
#include "scope_guard.h"
#include "connection.h"
#include "context.h"
#include "agenda.h"
#include "sync.h"
#include <sys/ioctl.h>
#include <boost/bind.hpp>

using namespace es3;
#include <curl/curl.h>

static int term_width = 80;

namespace po = boost::program_options;
typedef std::vector<std::string> stringvec;

int do_rsync(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help)
{
	bool delete_missing=false;
	po::options_description opts("Sync options", term_width);
	opts.add_options()
		("delete-missing,D", po::value<bool>(
			 &delete_missing)->default_value(false),
			"Delete missing files from the sync destination")
	;

	if (help)
	{
		std::cout << "Sync syntax: es3 sync [OPTIONS] <SOURCE> <DESTINATION>\n"
				  << "where <SOURCE> and <DESTINATION> are either:\n"
				  << "\t - Local directory\n"
				  << "\t - Amazon S3 storage (in s3://<bucket>/path/ format)"
				  << std::endl << std::endl;
		std::cout << opts;
		return 0;
	}

	po::positional_options_description pos;
	pos.add("<SOURCE>", 1);
	pos.add("<DESTINATION>", 1);

	std::string src, tgt;
	opts.add_options()
		("<SOURCE>", po::value<std::string>(&src)->required())
		("<DESTINATION>", po::value<std::string>(&tgt)->required())
	;

	try
	{
		po::variables_map vm;
		po::store(po::command_line_parser(params)
			.options(opts).positional(pos).run(), vm);
		po::notify(vm);
	} catch(const boost::program_options::error &err)
	{
		std::cerr << "Failed to parse configuration options. Error: "
				  << err.what() << "\n"
				  << "Use --help for help\n";
		return 2;
	}

	s3_path path;
	std::string local;
	bool upload = false;
	if (src.find("s3://")==0)
	{
		path = parse_path(src);
		local = tgt;
		upload = false;
	} else if (tgt.find("s3://")==0)
	{
		path = parse_path(tgt);
		local = src;
		upload = true;
	} else
	{
		std::cerr << "Error: one of <SOURCE> or <DESTINATION> must be an S3 URL.\n";
		return 2;
	}
	if (local.find("s3://")==0)
	{
		std::cerr << "Error: one of <SOURCE> or <DESTINATION> must be a local path \n";
		return 2;
	}

	//TODO: de-uglify
	context->bucket_=path.bucket_;
	context->zone_="s3";
	s3_connection conn(context);
	std::string region=conn.find_region();
	if (!region.empty())
		context->zone_="s3-"+region;

	synchronizer sync(ag, context, path.path_, local, upload, delete_missing);
	sync.create_schedule();
	return ag->run();
}

std::vector<po::option> subcommands_parser(stringvec& args,
										   const stringvec& subcommands)
{
	std::vector<po::option> result;
	if (args.empty())
		return result;

	stringvec::const_iterator i(args.begin());
	stringvec::const_iterator cmd_idx=std::find(subcommands.begin(),
												subcommands.end(),*i);
	if (cmd_idx!=subcommands.end())
	{
		po::option opt;
		opt.string_key = "subcommand";
		opt.value.push_back(*i);
		opt.original_tokens.push_back(*i);
		result.push_back(opt);

		for (++i; i != args.end(); ++i)
		{
			po::option opt;
			opt.string_key = "subcommand_params";
			opt.value.push_back(*i);
			opt.original_tokens.push_back(*i);
			result.push_back(opt);
		}
		args.clear();
	}
	return result;
}

int main(int argc, char **argv)
{
	int verbosity = 0;

	//Get terminal size (to pretty-print help text)
	struct winsize w={0};
	ioctl(0, TIOCGWINSZ, &w);
	term_width=(w.ws_col==0)? 80 : w.ws_col;

	context_ptr cd(new conn_context());

	po::options_description generic("Generic options", term_width);
	generic.add_options()
		("help", "Display this message")
		("config,c", po::value<std::string>(),
			"Path to a file that contains configuration settings")
		("verbosity,v", po::value<int>(&verbosity)->default_value(1),
			"Verbosity level [0 - the lowest, 9 - the highest]")
		("no-progress,q", "Quiet mode (no progress indicator)")
		("no-stats,t", "Quiet mode (no final stats)")
		("scratch-dir,i", po::value<bf::path>(&cd->scratch_dir_)
			->default_value(bf::temp_directory_path())->required(),
			"Path to the scratch directory")
	;

	po::options_description access("Access settings", term_width);
	access.add_options()
		("access-key,a", po::value<std::string>(
			 &cd->api_key_)->required(),
			"Amazon S3 API key")
		("secret-key,s", po::value<std::string>(
			 &cd->secret_key)->required(),
			"Amazon S3 secret key")
		("use-ssl,l", po::value<bool>(
			 &cd->use_ssl_)->default_value(false),
			"Use SSL for communications with the Amazon S3 servers")
		("compression,m", po::value<bool>(
			 &cd->do_compression_)->default_value(true)->required(),
			"Use GZIP compression")
	;
	generic.add(access);

	int thread_num=0, io_threads=0, cpu_threads=0, segment_size=0, segments=0;
	po::options_description tuning("Tuning", term_width);
	tuning.add_options()
		("thread-num,n", po::value<int>(&thread_num)->default_value(0),
			"Number of download/upload threads used [0 - autodetect]")
		("reader-threads,r", po::value<int>(
			 &io_threads)->default_value(0),
			"Number of filesystem reader/writer threads [0 - autodetect]")
		("compressor-threads,o", po::value<int>(
			 &cpu_threads)->default_value(0),
			"Number of compressor threads [0 - autodetect]")
		("segment-size,g", po::value<int>(
			&segment_size)->default_value(0),
			"Segment size in bytes [0 - autodetect, 6291456 - minimum]")
		("segments-in-flight,f", po::value<int>(
			 &segments)->default_value(0),
			"Number of segments in-flight [0 - autodetect]")
	;
	generic.add(tuning);

	po::options_description sub_data("Subcommands");
	sub_data.add_options()
		("subcommand", po::value<std::string>())
		("subcommand_params", po::value<stringvec>()->multitoken());

	std::map< std::string,
			std::function<int(context_ptr, const stringvec&, agenda_ptr, bool)> >
			subcommands_map;
	subcommands_map["sync"] = boost::bind(&do_rsync, _1, _2, _3, _4);

	//	subcommands.push_back("ls");
	//	subcommands.push_back("cp");
	//	subcommands.push_back("rm");
	//	subcommands.push_back("mb");
	//	subcommands.push_back("rb");

	stringvec subcommands;
	for(auto iter=subcommands_map.begin();iter!=subcommands_map.end();++iter)
		subcommands.push_back(iter->first);

	std::string cur_subcommand;
	stringvec cur_sub_params;

	po::variables_map vm;
	try
	{
		sub_data.add(generic);
		po::parsed_options parsed = po::command_line_parser(argc, argv)
				.options(sub_data)
				.extra_style_parser(boost::bind(&subcommands_parser, _1,
												subcommands))
			.run();
		po::store(parsed, vm);

		cur_subcommand=vm.count("subcommand")==0?
					"" : vm["subcommand"].as<std::string>();
		cur_sub_params=vm.count("subcommand_params")==0?
					stringvec() : vm["subcommand_params"].as<stringvec>();

		if (argc < 2 || vm.count("help"))
		{
			if (cur_subcommand.empty())
			{
				std::cout << "Extreme S3 - fast S3 client\n" << generic
						  << "\nThe following commands are supported:\n\t";
				for(auto iter=subcommands.begin();iter!=subcommands.end();++iter)
					std::cout<< *iter <<" ";
				std::cout << "\nUse --help <command_name> to get more info\n";
			} else
			{
				std::cout << "Extreme S3 - fast S3 client\n";
				subcommands_map[cur_subcommand](cd, cur_sub_params,
												agenda_ptr(), true);
			}
			return 1;
		}

		if (cur_subcommand.empty())
		{
			std::cout << "No command specified. Use --help for help\n";
			return 2;
		}
	} catch(const boost::program_options::error &err)
	{
		std::cerr << "Failed to parse command line. Error: "
				  << err.what() << std::endl;
		return 2;
	}

	try
	{
		if (vm.count("config"))
		{
			// Parse the file and store the options
			std::string config_file = vm["config"].as<std::string>();
			po::store(po::parse_config_file<char>(config_file.c_str(),generic), vm);
		} else if (getenv("ES3_CONFIG"))
		{
			po::store(po::parse_config_file<char>(
						  getenv("ES3_CONFIG"),generic), vm);
		} else
		{
			const char *home=getenv("HOME");
			if (home)
			{
				bf::path cfg=bf::path(home) / ".es3cfg";
				if (bf::exists(cfg))
					po::store(po::parse_config_file<char>(
								  cfg.c_str(),generic), vm);
			}
		}
	} catch(const boost::program_options::error &err)
	{
		std::cerr << "Failed to parse the configuration file. Error: "
				  << err.what() << std::endl;
		return 2;
	}
	bool no_progress = vm.count("no-progress");
	bool no_stats = vm.count("no-stats");

	try
	{
		po::notify(vm);
	} catch(const boost::program_options::required_option &option)
	{
		std::cerr << "Required option " << option.get_option_name()
				  << " is not present." << std::endl;
		return 1;
	}

	logger::set_verbosity(verbosity);
	curl_global_init(CURL_GLOBAL_ALL);
	ON_BLOCK_EXIT(&curl_global_cleanup);

	if (segments>MAX_IN_FLIGHT || segments<=0)
		segments=MAX_IN_FLIGHT;
	if (segment_size<MIN_SEGMENT_SIZE)
		segment_size=MIN_SEGMENT_SIZE;
	if (cpu_threads<=0)
		cpu_threads=sysconf(_SC_NPROCESSORS_ONLN)+2;
	if (io_threads<=0)
		io_threads=sysconf(_SC_NPROCESSORS_ONLN)*2;
	if (thread_num<=0)
		thread_num=sysconf(_SC_NPROCESSORS_ONLN)*6+8;

	agenda_ptr ag(new agenda(thread_num, cpu_threads, io_threads,
							 no_progress, no_stats,
							 segment_size, segments));

	try
	{
		return subcommands_map[cur_subcommand](cd, cur_sub_params, ag, false);
	} catch(const std::exception &ex)
	{
		VLOG(0) << "Unexpected error: " << ex.what() << std::endl;
		return 8;
	}
}
