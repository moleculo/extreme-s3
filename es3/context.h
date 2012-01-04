#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include <condition_variable>
#include <boost/enable_shared_from_this.hpp>

#define MAX_SEGMENTS 9999

namespace es3 {

	struct segment
	{
		std::vector<char> data_;
		uint64_t pos_;
	};
	typedef boost::shared_ptr<segment> segment_ptr;

	class conn_context : public boost::enable_shared_from_this<conn_context>
	{
	public:
		bool use_ssl_;
		std::string zone_;
		std::string bucket_;
		std::string api_key_, secret_key;
		std::string local_root_, remote_root_;
		std::string scratch_path_;
		bool upload_;
		bool delete_missing_;

		int max_in_flight_,
			max_compressors_,
			max_readers_,
			segment_size_;

		conn_context();
		void validate();
		segment_ptr get_segment();
	private:
		conn_context(const conn_context &);

		std::mutex m_;
		std::condition_variable condition_;
		size_t in_flight_;

		friend struct segment_deleter;
	};
	typedef boost::shared_ptr<conn_context> context_ptr;

	bool should_compress(const std::string &p, uint64_t sz);
}; //namespace es3

#endif //CONTEXT_H
