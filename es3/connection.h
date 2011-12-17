#ifndef CONNECTION_H
#define CONNECTION_H

#include "common.h"

typedef void CURL;
struct curl_slist;

namespace es3 {

	struct connection_data
	{
		bool use_ssl_;
		std::string bucket_;
		std::string api_key_, secret_key;
		bool upload_;
		bool delete_missing_;
	};

	typedef std::map<std::string, std::string> header_map_t;

	class s3_connection
	{
		CURL *curl_;
		const connection_data conn_data_;
//		const std::string verb_, path_;
		const header_map_t opts_;
		struct curl_slist *header_list_;
	public:
		s3_connection(const connection_data &conn_data,
					  const std::string &verb,
					  const std::string &path,
					  const header_map_t &opts = header_map_t());
		~s3_connection();

		std::string read_fully();
	};

}; //namespace es3

#endif //CONNECTION_H
