#include <iostream>
#include <boost/program_options.hpp>
#include "common.h"
#include "scope_guard.h"
#include "connection.h"
#include "agenda.h"
#include "sync.h"

using namespace es3;
#include <curl/curl.h>

namespace po = boost::program_options;

int main(int argc, char **argv)
{
	int verbosity = 0, thread_num = 0;

	connection_data cd;
	cd.use_ssl_ = false;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "Display this message")
		("config,c", po::value<std::string>(),
			"Path to a file that contains configuration settings")
		("verbosity,v", po::value<int>(&verbosity)->default_value(0),
			"Verbosity level [0 - the lowest, 9 - the highest]")
		("thread-num,n", po::value<int>(&thread_num)->default_value(0),
			"Number of threads used [0 - autodetect]")
		("scratch-dir,r", po::value<std::string>(
			 &cd.scratch_path_)->default_value("/tmp")->required(),
			"Path to the scratch directory")

		("access-key,a", po::value<std::string>(
				 &cd.api_key_)->required(),
			"Amazon S3 API key")
		("secret-key,s", po::value<std::string>(
			 &cd.secret_key)->required(),
			"Amazon S3 secret key")
		("use-ssl,l", po::value<bool>(
			 &cd.use_ssl_)->default_value(false),
			"Use SSL for communications with the Amazon S3 servers")

		("do-upload,u", po::value<bool>(&cd.upload_)->default_value(true),
			"Upload local changes to the server")
		("delete-missing,d", po::value<bool>(
			 &cd.delete_missing_)->default_value(false),
			"Delete missing files from the remote side")

		("sync-dir,i", po::value<std::string>(
			 &cd.local_root_)->required(),
			"Local directory")
		("bucket-name,o", po::value<std::string>(
			 &cd.bucket_)->required(),
			"Name of Amazon S3 bucket")
		("remote-path,p", po::value<std::string>(
			 &cd.remote_root_)->default_value("/")->required(),
			"Path in the Amazon S3 bucket")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);

	if (argc < 2 || vm.count("help"))
	{
		std::cout << "Extreme S3 - fast rsync\n" << desc;
		return 1;
	}

	if (vm.count("config"))
	{
		// Parse the file and store the options
		std::string config_file = vm["config"].as<std::string>();
		po::store(po::parse_config_file<char>(config_file.c_str(),desc), vm);
	}

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

	agenda_ptr ag=agenda::make_new(thread_num);
	synchronizer sync(ag, cd);
	sync.create_schedule();
	ag->run();

	return 0;
}
