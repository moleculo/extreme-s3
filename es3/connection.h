#ifndef CONNECTION_H
#define CONNECTION_H

#include "common.h"
#include <boost/weak_ptr.hpp>

typedef void CURL;
struct curl_slist;

namespace es3 {
	class conn_context;
	typedef boost::shared_ptr<conn_context> context_ptr;

	typedef std::map<std::string, std::string, ci_string_less> header_map_t;

	struct remote_file;
	typedef boost::shared_ptr<remote_file> remote_file_ptr;
	typedef boost::weak_ptr<remote_file> remote_file_weak_t;
	typedef std::map<std::string, remote_file_ptr> file_map_t;

	struct remote_file
	{
		std::string name_, full_name_;
		uint64_t size_;

		bool is_dir_;
		file_map_t children_;
		remote_file_weak_t parent_;
	};

	struct file_desc
	{
		time_t mtime_;
		uint64_t raw_size_, remote_size_;
		mode_t mode_;
		bool compressed;
	};

	class s3_connection
	{
		CURL *curl_;
		char error_buffer_[256+1]; //CURL_ERROR_SIZE+1
		const context_ptr conn_data_;
		struct curl_slist *header_list_;
	public:
		s3_connection(const context_ptr &conn_data);
		~s3_connection();

		std::string read_fully(const std::string &verb,
							   const std::string &path,
							   const std::string &args="",
							   const header_map_t &opts=header_map_t());
		file_map_t list_files(const std::string &path,
							  const std::string &prefix);
		std::string upload_data(const std::string &path,
								const char *data, uint64_t size,
								const header_map_t& opts=header_map_t());
		void download_data(const std::string &path,
			uint64_t offset, char *data, uint64_t size,
			const header_map_t& opts=header_map_t());

		std::string initiate_multipart(const std::string &path,
									   const header_map_t &opts);
		std::string complete_multipart(const std::string &path,
									   const std::string &upload_id,
									   const std::vector<std::string> &etags);
		file_desc find_mtime_and_size(const std::string &path);

		std::string find_region();
	private:
		void checked(int curl_code);
		void check_for_errors(const std::string &curl_res);
		void prepare(const std::string &verb,
				  const std::string &path,
				  const header_map_t &opts=header_map_t());

		std::string sign(const std::string &str);
		struct curl_slist* authenticate_req(struct curl_slist *,
				const std::string &verb, const std::string &path,
				const header_map_t &opts);

		void set_url(const std::string &path, const std::string &args);
		void deconstruct_file(file_map_t &res, const std::string &name,
							  const std::string &size);
	};

}; //namespace es3

#endif //CONNECTION_H
