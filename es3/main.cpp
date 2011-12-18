#include "common.h"
#include <gflags/gflags.h>
#include "scope_guard.h"
#include <iostream>
#include "connection.h"

using namespace es3;
#include <curl/curl.h>

DEFINE_string(access_key, "", "Access key");
DEFINE_string(secret_key, "", "Secret key");
DEFINE_bool(do_upload, false, "Do upload");
DEFINE_bool(delete_missing, false, "Delete missing");

DEFINE_string(sync_dir, "", "Local directory");
DEFINE_string(bucket_name, "", "Bucket name");
DEFINE_string(remote_path, "", "Remote path");

int main(int argc, char **argv)
{
	ON_BLOCK_EXIT(&google::ShutDownCommandLineFlags);
	google::SetUsageMessage("Usage");
	google::SetVersionString("ExtremeS3 0.1");
	google::ParseCommandLineFlags(&argc, &argv, true);

	curl_global_init(CURL_GLOBAL_ALL);
	ON_BLOCK_EXIT(&curl_global_cleanup);

	connection_data cd;
	cd.use_ssl_ = false;
	cd.upload_ = false;
	cd.bucket_ = FLAGS_bucket_name;
	cd.secret_key = FLAGS_secret_key;
	cd.api_key_ = FLAGS_access_key;
	cd.delete_missing_ = true;

	s3_connection conn(cd, "GET", "/");
	std::cout<<conn.read_fully()<<std::endl;

	return 0;
}
