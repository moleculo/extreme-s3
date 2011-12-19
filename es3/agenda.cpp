#include "agenda.h"

#include <unistd.h>
#include <boost/threadpool.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <functional>
#include <openssl/md5.h>

using namespace es3;
using namespace boost::interprocess;

//Utility thread pool
boost::threadpool::pool the_pool(
	sysconf(_SC_NPROCESSORS_ONLN)*2);

agenda_ptr agenda::make_new()
{
	return agenda_ptr(new agenda());
}

agenda::agenda()
{

}

void agenda::schedule(sync_task_ptr task)
{
}

void agenda::schedule_removal(remote_file_ptr file)
{

}

class file_uploader : public sync_task
{
	const connection_data conn_;
	const boost::filesystem::path path_;
	const std::string remote_;
	const std::string etag_;

public:
	file_uploader(const connection_data &conn,
				  const boost::filesystem::path &path,
				  const std::string &remote,
				  const std::string &etag)
		: conn_(conn), path_(path), remote_(remote), etag_(etag)
	{

	}

	virtual result_code_t operator()()
	{
		//Create a file mapping.
		file_mapping m_file(path_.string().c_str(), read_only);
		//Map the whole file in this process
		mapped_region region(m_file,read_only);
		void * addr = region.get_address();
		std::size_t size = region.get_size();
		//Get the address of the mapped region

		VLOG(1) << "Checking upload of " << path_ << " as "
				  << remote_;
		if (etag_.empty())
		{
			start_upload("", addr, size);
		} else
		{
			static const char alphabet[17]="0123456789abcdef";
			unsigned char res[MD5_DIGEST_LENGTH+1]={0};
			MD5((const unsigned char*)addr, size, res);

			std::string md5_str;
			md5_str.resize(MD5_DIGEST_LENGTH*2);
			for(int f=0;f<MD5_DIGEST_LENGTH;++f)
			{
				md5_str[f*2]=alphabet[res[f]/16];
				md5_str[f*2+1]=alphabet[res[f]%16];
			}
			std::string md5_web;
			md5_web.push_back('\"');
			md5_web.append(md5_str);
			md5_web.push_back('\"');
			if (md5_web == etag_)
				return result_code_t();

			start_upload(md5_web, addr, size);
		}

		return result_code_t();
	}

	virtual std::string describe() const
	{
		return "";
	}
private:
	void start_upload(const std::string &md5, void *addr, size_t size)
	{
		VLOG(0) << "Starting upload of " << path_ << " as "
				<< remote_ << ", MD5: "<< md5;
		header_map_t hmap;
		if (!md5.empty())
			hmap["Content-MD5"] = md5;
		hmap["Content-Type"] = "application/x-binary";
		s3_connection up(conn_, "PUT", remote_, hmap);
		up.upload_data(addr, size);
	}
};

struct task_executor
{
	sync_task_ptr task_;
	void operator ()()
	{
		task_->operator()();
	}
};

void agenda::schedule_upload(const connection_data &data,
							 const boost::filesystem::path &path,
							 const std::string &remote,
							 const std::string &etag)
{
	sync_task_ptr task(new file_uploader(data, path, remote, etag));
	the_pool.schedule(task_executor {task});
}
