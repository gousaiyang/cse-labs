#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#define MAX_FILENAME 255

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
    /*static inum s2n(std::string);
    static std::string n2s(inum);*/
    static bool filename_valid(std::string);
    static bool inum_valid(inum);
    int writedir(inum, std::list<dirent> &);

public:
    yfs_client();
    yfs_client(std::string, std::string);
    ~yfs_client();

    bool isfile(inum);
    bool isdir(inum);

    int getfile(inum, fileinfo &);
    int getdir(inum, dirinfo &);

    int setattr(inum, size_t);
    int lookup(inum, const char *, bool &, inum &);
    int create(inum, const char *, mode_t, inum &);
    int readdir(inum, std::list<dirent> &);
    int write(inum, size_t, off_t, const char *, size_t &);
    int read(inum, size_t, off_t, std::string &);
    int unlink(inum, const char *);
    int mkdir(inum , const char *, mode_t , inum &);

    /** you may need to add symbolic link related methods here.*/
};

#endif
