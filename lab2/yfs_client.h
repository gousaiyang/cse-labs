#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#define MAX_FILENAME 255 // Maximum file name length allowed.

class yfs_client {
    extent_client *ec;

public:
    typedef unsigned long long inum;
    enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
    typedef int status;

    struct fileinfo {
        unsigned long long size;
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };
    struct dirinfo {
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };
    struct dirent {
        std::string name;
        yfs_client::inum inum;
    };

private:
    static bool filename_valid(std::string);
    static bool inum_valid(inum);
    int writedir(inum, std::list<dirent> &);
    int createitem(inum, const char *, mode_t, inum &, uint32_t);
    bool istype(inum, uint32_t);

public:
    yfs_client();
    yfs_client(std::string, std::string);
    ~yfs_client();

    bool isfile(inum);
    bool isdir(inum);
    bool issymlink(inum);

    int getfile(inum, fileinfo &);
    int getdir(inum, dirinfo &);
    int getsymlink(inum, fileinfo &);

    int setattr(inum, size_t);
    int lookup(inum, const char *, bool &, inum &);
    int create(inum, const char *, mode_t, inum &);
    int readdir(inum, std::list<dirent> &);
    int write(inum, size_t, off_t, const char *, size_t &);
    int read(inum, size_t, off_t, std::string &);
    int unlink(inum, const char *);
    int mkdir(inum, const char *, mode_t, inum &);
    int symlink(inum, const char *, const char *, inum &);
    int readlink(inum, std::string &);

    /** you may need to add symbolic link related methods here.*/
};

#endif
