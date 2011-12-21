#include "common.h"
#include <gflags/gflags.h>
#include "scope_guard.h"
#include <iostream>
#include "connection.h"
#include "agenda.h"
#include "sync.h"

using namespace es3;
#include <curl/curl.h>

DEFINE_string(access_key, "", "Access key");
DEFINE_string(secret_key, "", "Secret key");
DEFINE_bool(do_upload, false, "Do upload");
DEFINE_bool(delete_missing, false, "Delete missing");

DEFINE_string(sync_dir, ".", "Local directory");
DEFINE_string(bucket_name, "", "Bucket name");
DEFINE_string(remote_path, "", "Remote path");

int main(int argc, char **argv)
{
	ON_BLOCK_EXIT(&google::ShutDownCommandLineFlags);
	google::SetUsageMessage("Usage");
	google::SetVersionString("ExtremeS3 0.1");
	google::ParseCommandLineFlags(&argc, &argv, true);

	if (FLAGS_bucket_name.empty() || FLAGS_access_key.empty() ||
			FLAGS_secret_key.empty())
	{
		printf("Not enough arguments. Use --help for help.\n");
		return 1;
	}

	curl_global_init(CURL_GLOBAL_ALL);
	ON_BLOCK_EXIT(&curl_global_cleanup);

	connection_data cd;
	cd.use_ssl_ = false;
	cd.upload_ = false;
	cd.bucket_ = FLAGS_bucket_name;
	cd.local_root_ = FLAGS_sync_dir;
	cd.remote_root_ = FLAGS_remote_path;
	cd.secret_key = FLAGS_secret_key;
	cd.api_key_ = FLAGS_access_key;
	cd.delete_missing_ = true;

	agenda_ptr ag=agenda::make_new();
	synchronizer sync(ag, cd);
	sync.create_schedule();

	return 0;
}
