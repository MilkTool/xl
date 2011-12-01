#ifndef UTF8_IFSTREAM_H
#define UTF8_IFSTREAM_H
// ****************************************************************************
//  utf8_ifstream.h                                                XLR project
// ****************************************************************************
//
//   File Description:
//
//    A class similar to ifstream, with support for UTF-8 encoded filenames.
//
//
//
//
//
//
//
//
// ****************************************************************************
// This document is released under the GNU General Public License.
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
//  (C) 2011 Jerome Forissier <jerome@taodyne.com>
//  (C) 2011 Taodyne SAS
// ****************************************************************************

#include <fstream>
#include <sys/stat.h>


#ifdef CONFIG_MINGW

#include <ext/stdio_filebuf.h>
#include <windows.h>
#include <fcntl.h>

inline std::wstring utf8_decode(const std::string &str)
// ----------------------------------------------------------------------------
//    Convert a UTF-8 std::string into a UTF-16 std::wstring
// ----------------------------------------------------------------------------
{
    int needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(),
                                     NULL, 0);
    std::wstring dest(needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(),
                        &dest[0], needed);
    return dest;
}


class utf8_ifstream : public std::istream
// ----------------------------------------------------------------------------
//    Quick-and-dirty ifstream with support for UTF-8 encoded file names
// ----------------------------------------------------------------------------
{

public:
    typedef std::istream                   istream_type;
    typedef __gnu_cxx::stdio_filebuf<char> filebuf_type;

public:
    utf8_ifstream()
        : istream_type()
    {}

    explicit
    utf8_ifstream(const char* s,
                  std::ios::openmode mode = std::ios::in)
        : istream_type(&b),
          b(_wopen(utf8_decode(s).c_str(), wflags(mode)), mode)
    {}

protected:
    int wflags(std::ios::openmode mode)
    {
        int flags = _O_RDONLY;
        if (mode & std::ios::binary)
            flags |= _O_BINARY;
        return flags;
    }

protected:
    filebuf_type b;
};


inline time_t fileTimeToTime_t(FILETIME ft)
{
   ULARGE_INTEGER ull;
   ull.LowPart = ft.dwLowDateTime;
   ull.HighPart = ft.dwHighDateTime;
   return ull.QuadPart / 10000000ULL - 11644473600ULL;
}


inline int utf8_stat(const char *path, struct _stat *buffer)
{
    std::wstring wpath = utf8_decode(std::string(path));
    int status = _wstat(wpath.c_str(), buffer);
    if (status < 0)
        return status;
    // Bug#1567: _wsat returns bogus file times when daylight saving
    // time setting changes
    // By using GetFileAttributesEx we are consistent with
    // QFileInfo::lastModified().toTime_t().
    WIN32_FILE_ATTRIBUTE_DATA attr;
    bool ok = GetFileAttributesEx((const wchar_t *)wpath.c_str(),
                                  GetFileExInfoStandard, &attr);
    if (ok)
    {
        buffer->st_ctime = fileTimeToTime_t(attr.ftCreationTime);
        buffer->st_atime = fileTimeToTime_t(attr.ftLastAccessTime);
        buffer->st_mtime = fileTimeToTime_t(attr.ftLastWriteTime);
    }
    return 0;
}


inline int utf8_access(const char *path, int mode)
{
    std::wstring wpath = utf8_decode(std::string(path));
    return _waccess(wpath.c_str(), mode);
}

#else

typedef std::ifstream utf8_ifstream;
#define utf8_stat(path, buffer) stat(path, buffer)
#define utf8_access(path, mode) access(path, mode)

#endif

#endif // UTF8_FSTREAM_H