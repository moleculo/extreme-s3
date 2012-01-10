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

	po::variables_map vm;
	try
	{
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

	bool delete_missing=vm.count("delete-missing");

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

	synchronizer sync(ag, context, path.path_, local, upload, delete_missing,
					  included, excluded);
	sync.create_schedule();
	return ag->run();
}
