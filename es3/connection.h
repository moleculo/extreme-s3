#ifndef CONNECTION_H
#define CONNECTION_H

#include "common.h"
#include <boost/weak_ptr.hpp>

typedef void CURL;
struct curl_slist;

namespace es3 {

	struct ci_string_less :
		public std::binary_function<std::string, std::string, bool>
	{
		ES3LIB_PUBLIC bool operator()(const std::string &lhs,
									  const std::string &rhs) const;
	};

	template<typename K, typename V, typename C, typename A, typename K2>
		V try_get(const std::map<K,V,C,A> &map, const K2 &key, const V &def=V())
	{
		const typename std::map<K,V,C,A>::const_iterator pos=map.find(key);
		if (pos==map.end()) return def;
		return pos->second;
	}

	struct connection_data
	{
		bool use_ssl_;
		std::string bucket_;
		std::string api_key_, secret_key;
		std::string local_root_, remote_root_;
		std::string scratch_path_;
		bool upload_;
		bool delete_missing_;
	};

	typedef std::map<std::string, std::string, ci_string_less> header_map_t;

	struct remote_file;
	typedef boost::shared_ptr<remote_file> remote_file_ptr;
	typedef boost::weak_ptr<remote_file> remote_file_weak_t;
	typedef std::map<std::string, remote_file_ptr> file_map_t;

	struct remote_file
	{
		std::string name_, full_name_;
		std::string etag_;
		uint64_t size_;

		bool is_dir_;
		file_map_t children_;
		remote_file_weak_t parent_;
	};

	class s3_connection
	{
		CURL *curl_;
		const connection_data conn_data_;
		struct curl_slist *header_list_;
	public:
		s3_connection(const connection_data &conn_data);
//					  const header_map_t &opts = header_map_t());
		~s3_connection();

		std::string read_fully_def(const std::string &verb,
							   const std::string &path)
		{
			return read_fully(verb, path, header_map_t());
		}

		std::string read_fully(const std::string &verb,
							   const std::string &path,
							   const header_map_t &opts=header_map_t());
		file_map_t list_files(const std::string &path,
							  const std::string &prefix);
		std::string upload_data(const std::string &path,
			const handle_t &descriptor, uint64_t size, uint64_t offset,
			const header_map_t& opts);

		std::string initiate_multipart(const std::string &path,
									   const header_map_t &opts);
		std::string complete_multipart(const std::string &path,
			const std::vector<std::string> &etags);
		std::pair<time_t, uint64_t> find_mtime_and_size(
			const std::string &path);

	private:
		void prepare(const std::string &verb,
				  const std::string &path,
				  const header_map_t &opts=header_map_t());

		std::string sign(const std::string &str);
		struct curl_slist* authenticate_req(struct curl_slist *,
				const std::string &verb, const std::string &path,
				const header_map_t &opts);

		void set_url(const std::string &path, const std::string &args);
		void deconstruct_file(file_map_t &res, const std::string &name,
							  const std::string &etag,
							  const std::string &size);
	};

}; //namespace es3

#endif //CONNECTION_H
