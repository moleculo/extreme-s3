#include "downloader.h"
#include "workaround.hpp"
#include "context.h"

using namespace es3;
using namespace boost::filesystem;

void local_file_deleter::operator ()(agenda_ptr agenda_)
{
	VLOG(1) << "Removing " << file_;
	path p(file_);
	remove_all(p);
}

void file_downloader::operator()(agenda_ptr agenda)
{
	VLOG(2) << "Checking download of " << path_ << " from "
			  << remote_;
}
