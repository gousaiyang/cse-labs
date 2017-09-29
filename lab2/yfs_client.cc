// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::~yfs_client()
{
    delete ec;
}

/*yfs_client::inum yfs_client::s2n(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string yfs_client::n2s(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}*/

bool yfs_client::filename_valid(std::string name)
{
    return !name.empty() && name.length() <= MAX_FILENAME
        && name.find('/') == std::string::npos && name.find('\0') == std::string::npos;
}

bool yfs_client::inum_valid(inum inum)
{
    return inum >= 1 && inum <= INODE_NUM;
}

int yfs_client::writedir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    if (!inum_valid(dir))
        return IOERR;

    std::ostringstream ost;

    for (std::list<dirent>::iterator it = list.begin(); it != list.end(); ++it) {
        ost.put((unsigned char)it->name.length());
        ost.write(it->name.c_str(), it->name.length());
        ost.write((char*)&it->inum, sizeof(inum));
    }

    EXT_RPC(ec->put(dir, ost.str()));

release:
    return r;
}

bool yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (!inum_valid(inum))
        return false;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }
    printf("isfile: %lld is a dir\n", inum); // Other types?
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */

bool yfs_client::isdir(inum inum)
{
    if (!inum_valid(inum))
        return false;

    // Oops! This will be wrong when you implement symlink!
    return ! isfile(inum);
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    if (!inum_valid(inum))
        return IOERR;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    if (!inum_valid(inum))
        return IOERR;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}

// Only support set size of attr
int yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    return r;
}

// Dir entry layout: |<file name length>|<file name>|<inum>|

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent information.
     * You don't need to implement file mode.
     */

    if (!inum_valid(parent))
        return IOERR;

    std::string fname = std::string(name);

    if (!filename_valid(fname))
        return IOERR;

    std::list<dirent> itemlist;

    r = readdir(parent, itemlist);
    if (r != OK)
        return r;

    for (std::list<dirent>::iterator it = itemlist.begin(); it != itemlist.end(); ++it)
        if (it->name == fname)
            return EXIST;
    
    dirent de;
    de.name = fname;

    EXT_RPC(ec->create(extent_protocol::T_FILE, ino_out));
    de.inum = ino_out;
    
    itemlist.push_back(de);
    r = writedir(parent, itemlist);

release:
    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent information.
     */

    return r;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    if (!inum_valid(parent))
        return IOERR;

    std::string fname = std::string(name);
    found = false;

    if (!filename_valid(fname))
        return r;

    std::list<dirent> itemlist;

    r = readdir(parent, itemlist);
    if (r != OK)
        return r;

    for (std::list<dirent>::iterator it = itemlist.begin(); it != itemlist.end(); ++it) {
        if (it->name == fname) {
            found = true;
            ino_out = it->inum;
            break;
        }
    }

    return r;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the directory content using your defined format,
     * and push the dirents to the list.
     */

    list.clear();

    if (!inum_valid(dir))
        return IOERR;

    std::istringstream ist;
    std::string content;
    char c;
    char buf[MAX_FILENAME + 1];
    dirent de;

    bzero(buf, MAX_FILENAME + 1); // Clear it because istringstream::read will not append '\0'!
    EXT_RPC(ec->get(dir, content));

    ist.str(content);

    while (ist.get(c)) {
        if (!ist.read(buf, (int)(unsigned char)c)) // Note: istringstream::get(buf, size) will stop at '\n'!
            break;

        de.name = std::string(buf);

        if (!ist.read((char*)&de.inum, sizeof(inum)))
            break;

        list.push_back(de);
    }

release:
    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */

    return r;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data, size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    return r;
}
