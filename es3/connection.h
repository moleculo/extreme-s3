#ifndef CONNECTION_H
#define CONNECTION_H

#include "common.h"

typedef void CURL;
struct curl_slist;

namespace es3 {

	struct ci_string_less :
		public std::binary_function<std::string, std::string, bool>
	{
		bool operator()(const std::string &lhs, const std::string &rhs) const
		{
			return strcasecmp(lhs.c_str(), rhs.c_str()) < 0 ? 1 : 0;
		}
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
		std::string name_;
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
		const std::string path_;
		const header_map_t opts_;
		struct curl_slist *header_list_;
	public:
		s3_connection(const connection_data &conn_data,
					  const std::string &verb,
					  const std::string &path,
					  const header_map_t &opts = header_map_t());
		~s3_connection();

		std::string read_fully();
		file_map_t list_files(const std::string &prefix);
		void upload_data(void *addr, size_t size);

	private:
		std::string sign(const std::string &str);
		struct curl_slist* authenticate_req(struct curl_slist *,
				const std::string &verb, const std::string &path);

		void set_url(const std::string &args);
		void deconstruct_file(file_map_t &res, const std::string &name,
							  const std::string &etag,
							  const std::string &size);
	};

}; //namespace es3

#endif //CONNECTION_H
