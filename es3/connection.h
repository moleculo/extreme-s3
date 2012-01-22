#ifndef CONNECTION_H
#define CONNECTION_H

#include "common.h"
#include <functional>
#include <boost/weak_ptr.hpp>

typedef void CURL;
struct curl_slist;

namespace es3 {
	class conn_context;
	typedef boost::shared_ptr<conn_context> context_ptr;
	typedef std::map<std::string, std::string, ci_string_less> header_map_t;

	struct file_desc
	{
		time_t mtime_;
		uint64_t raw_size_, remote_size_;
		mode_t mode_;
		bool compressed;
	};

	struct s3_path
	{
		std::string zone_, bucket_, path_;
	};
	inline s3_path derive(const s3_path &left, const std::string &right)
	{
		s3_path res(left);
		if (right.empty())
			return res;

		if (*res.path_.rbegin()!='/' && right.at(0)!='/')
			res.path_.append("/");
		res.path_.append(right);
		return res;
	}
	ES3LIB_PUBLIC s3_path parse_path(const std::string &url);
	inline std::ostream& operator << (std::ostream &out, const s3_path &p)
	{
		out << "s3://" << p.bucket_ << "/" << p.path_;
		return out;
	}

	struct s3_file;
	typedef boost::shared_ptr<s3_file> s3_file_ptr;
	typedef std::map<std::string, s3_file_ptr> file_map_t;

	struct s3_directory;
	typedef boost::shared_ptr<s3_directory> s3_directory_ptr;
	typedef boost::weak_ptr<s3_directory> s3_directory_weak_t;
	typedef std::map<std::string, s3_directory_ptr> subdir_map_t;

	struct s3_directory
	{
		std::string name_;
		s3_path absolute_name_;

		file_map_t files_;
		subdir_map_t subdirs_;
		s3_directory_weak_t parent_;
	};

	struct s3_file
	{
		std::string name_;
		s3_path absolute_name_;

		uint64_t size_;
		s3_directory_weak_t parent_;
	};

	typedef std::function<void(size_t)> progress_callback_t;

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
							   const s3_path &path,
							   const std::string &args="",
							   const header_map_t &opts=header_map_t());
		s3_directory_ptr list_files(const s3_path &path, bool try_to_root);
		std::string upload_data(const s3_path &path,
								const char *data, size_t size,
								const header_map_t& opts=header_map_t());
		void download_data(const s3_path &path,
			uint64_t offset, char *data, size_t size,
			const header_map_t& opts=header_map_t());

		std::string initiate_multipart(const s3_path &path,
									   const header_map_t &opts);
		std::string complete_multipart(const s3_path &path,
									   const std::string &upload_id,
									   const std::vector<std::string> &etags);
		file_desc find_mtime_and_size(const s3_path &path);

		std::string find_region(const std::string &bucket);
	private:
		void checked(int curl_code);
		void check_for_errors(const std::string &curl_res);
		void prepare(const std::string &verb,
				  const s3_path &path,
				  const header_map_t &opts=header_map_t());

		std::string sign(const std::string &str);
		struct curl_slist* authenticate_req(struct curl_slist *,
				const std::string &verb, const s3_path &path,
				const header_map_t &opts);

		void set_url(const s3_path &path, const std::string &args);
		void deconstruct_file(s3_directory_ptr res, const std::string &name,
							  const std::string &size);
	};

}; //namespace es3

#endif //CONNECTION_H
