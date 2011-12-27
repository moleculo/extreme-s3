#ifndef WORkAROUND_B3
#define WORkAROUND_B3

#include <boost/filesystem.hpp>
//Boost.Filesystem incompatibilities workaround

#if BOOST_FILESYSTEM_VERSION!=3
inline std::string get_file(const std::string &name)
{
    return name;
}
#else
inline std::string get_file(const boost::filesystem3::path &name)
{
    return name.string();
}
#endif

#endif //WORkAROUND_B3
