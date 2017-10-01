// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static inline size_t maxsize2(size_t s1, size_t s2) {
    return s1 > s2 ? s1 : s2;
}

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

bool yfs_client::filename_valid(std::string name)
{
    return !name.empty() && name.length() <= MAX_FILENAME
        && name.find('/') == std::string::npos && name.find('\0') == std::string::npos;
}

bool yfs_client::inum_valid(inum inum)
{
    return inum >= 1 && inum <= INODE_NUM;
}

// Dir entry layout: |<file name length>|<file name>|<inum>|

int yfs_client::writedir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    if (!isdir(dir))
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

int yfs_client::createitem(inum parent, const char *name, mode_t mode, inum &ino_out, uint32_t type)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file or dir exist;
     * after create file or dir, you must remember to modify the parent information.
     * Ignore mode.
     */

    if (!isdir(parent))
        return IOERR;

    if (!name)
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

    EXT_RPC(ec->create(type, ino_out));
    de.inum = ino_out;
    
    itemlist.push_back(de);
    r = writedir(parent, itemlist);

release:
    return r;
}

bool yfs_client::istype(inum inum, uint32_t type)
{
    extent_protocol::attr a;

    if (!inum_valid(inum))
        return false;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    return a.type == type;
}

bool yfs_client::isfile(inum inum)
{
    if (istype(inum, extent_protocol::T_FILE)) {
        printf("isfile: %llu is a file\n", inum);
        return true;
    }

    return false;
}

bool yfs_client::isdir(inum inum)
{
    if (istype(inum, extent_protocol::T_DIR)) {
        printf("isdir: %llu is a dir\n", inum);
        return true;
    }

    return false;
}

bool yfs_client::issymlink(inum inum)
{
    if (istype(inum, extent_protocol::T_SYMLINK)) {
        printf("issymlink: %llu is a symlink\n", inum);
        return true;
    }

    return false;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    if (!inum_valid(inum))
        return IOERR;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    if (a.type != extent_protocol::T_FILE)
        return IOERR;

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

    if (a.type != extent_protocol::T_DIR)
        return IOERR;

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

    if (!inum_valid(ino))
        return IOERR;

    size_t csize;
    std::string content;

    EXT_RPC(ec->get(ino, content));

    csize = content.length();

    EXT_RPC(ec->put(ino, size > csize ? (content + std::string(size - csize, '\0')) : content.substr(0, size)));

release:
    return r;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    return createitem(parent, name, mode, ino_out, extent_protocol::T_FILE);
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    return createitem(parent, name, mode, ino_out, extent_protocol::T_DIR);
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    found = false;

    if (!isdir(parent))
        return IOERR;

    if (!name)
        return r;

    std::string fname = std::string(name);

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

    if (!isdir(dir))
        return IOERR;

    std::istringstream ist;
    std::string content;
    char c;
    char buf[MAX_FILENAME + 1];
    dirent de;
    int namelen;
    
    EXT_RPC(ec->get(dir, content));

    ist.str(content);

    while (ist.get(c)) {
        namelen = (int)(unsigned char)c;

        if (!ist.read(buf, namelen)) // Note: istringstream::get(buf, namelen) will stop at '\n'!
            break;

        de.name = std::string(buf, namelen); // Specify size because istringstream::read will not append '\0'!

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

    if (!isfile(ino)) {
        data.clear();
        return IOERR;
    }

    std::string content;
    EXT_RPC(ec->get(ino, content));
    data = content.substr(off, size);

release:
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

    bytes_written = 0;

    if (!isfile(ino))
        return IOERR;

    if (!data)
        return r;

    size_t newsize;
    std::string content, newcontent;
    char *buf;

    EXT_RPC(ec->get(ino, content));

    newsize = maxsize2(content.length(), off + size);

    buf = (char*)calloc(newsize, 1);
    if (!buf) {
        printf("Error: calloc failed.\n");
        exit(-1);
    }

    memcpy(buf, content.c_str(), content.length());
    memcpy(buf + off, data, size);
    newcontent = std::string(buf, newsize); // We have to specify size, or the construction may stop at '\0'.
    free(buf);

    EXT_RPC(ec->put(ino, newcontent));
    bytes_written = size;

release:
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

    if (!isdir(parent))
        return IOERR;

    if (!name)
        return NOENT;

    std::string fname = std::string(name);

    if (!filename_valid(fname))
        return NOENT;
    
    std::list<dirent> itemlist;

    r = readdir(parent, itemlist);
    if (r != OK)
        return r;

    std::list<dirent>::iterator it;

    for (it = itemlist.begin(); it != itemlist.end(); ++it)
        if (it->name == fname) 
            break;

    if (it == itemlist.end())
        return NOENT;

    if (!isfile(it->inum) && !issymlink(it->inum))
        return IOERR;

    EXT_RPC(ec->remove(it->inum));
    itemlist.erase(it);
    
    r = writedir(parent, itemlist);

release:
    return r;
}
