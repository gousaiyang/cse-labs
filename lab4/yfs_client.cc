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

// Macros for RPC error handling.

#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

#define LCK_RPC(xx, r) do { \
    if ((xx) != lock_protocol::OK) { \
        printf("LCK_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        return (r); \
    } \
} while (0)

/*yfs_client::yfs_client()
{
    ec = new extent_client();
}*/

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    lc = new lock_client(lock_dst);
}

yfs_client::~yfs_client()
{
    delete ec;
    delete lc;
}

bool yfs_client::filename_valid(std::string name)
{
    return !name.empty() && name.length() <= MAX_FILENAME
           && name.find('/') == std::string::npos && name.find('\0') == std::string::npos;
}

bool yfs_client::inum_valid(inum inum) // Check whether inum is in the possible range.
{
    return inum >= 1 && inum <= INODE_NUM;
}

// Dir entry layout: |<file name length>|<file name>|<inum>|

int yfs_client::readdir_p(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the directory content using your defined format,
     * and push the dirents to the list.
     */

    list.clear();

    if (!isdir_p(dir))
        return IOERR;

    std::istringstream ist;
    std::string content;
    char c;
    char buf[MAX_FILENAME + 1];
    dirent de;
    int namelen;

    EXT_RPC(ec->get(dir, content));

    ist.str(content);

    while (ist.get(c)) { // Read next file name length.
        namelen = (int)(unsigned char)c;

        // Read file name.
        // Note: not using ist.get(buf, namelen) because it will stop at '\n'!
        if (!ist.read(buf, namelen))
            break;

        de.name = std::string(buf, namelen); // Specify size because istringstream::read will not append '\0'.

        // Read inode number.
        if (!ist.read((char*)&de.inum, sizeof(inum)))
            break;

        list.push_back(de);
    }

release:
    return r;
}

int yfs_client::writedir(inum dir, std::list<dirent> &list) // Write the directory entry table.
{
    int r = OK;

    if (!isdir_p(dir))
        return IOERR;

    std::ostringstream ost;

    for (std::list<dirent>::iterator it = list.begin(); it != list.end(); ++it) {
        if (!ost.put((unsigned char)it->name.length()) ||
                !ost.write(it->name.c_str(), it->name.length()) ||
                !ost.write((char*)&it->inum, sizeof(inum))) {
            printf("Error: ostringstream write failed.\n");
            return IOERR;
        }
    }

    EXT_RPC(ec->put(dir, ost.str()));

release:
    return r;
}

// Helper function to create a given type of inode.
int yfs_client::createitem(inum parent, const char *name, mode_t mode, inum &ino_out, uint32_t type)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file or dir exist;
     * after create file or dir, you must remember to modify the parent information.
     * Ignore mode.
     */

    // Check input parameters.
    if (!name)
        return IOERR;

    std::string fname = std::string(name);

    if (!filename_valid(fname))
        return IOERR;

    if (!isdir_p(parent))
        return IOERR;

    // Read the directory entries.
    std::list<dirent> itemlist;

    r = readdir_p(parent, itemlist);
    if (r != OK)
        return r;

    // Check whether the name already exist.
    for (std::list<dirent>::iterator it = itemlist.begin(); it != itemlist.end(); ++it)
        if (it->name == fname)
            return EXIST;

    dirent de;
    de.name = fname;

    // Allocate a new inode.
    LCK_RPC(lc->acquire(CREATE_LOCK_ID), IOERR); // Lock for create inode operations.
    EXT_RPC(ec->create(type, ino_out));
    de.inum = ino_out;

    // Add the new entry to the directory and write back.
    itemlist.push_back(de);
    r = writedir(parent, itemlist);

release:
    LCK_RPC(lc->release(CREATE_LOCK_ID), IOERR);
    return r;
}

bool yfs_client::istype(inum inum, uint32_t type) // Helper function to check inode type.
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

bool yfs_client::isfile_p(inum inum)
{
    if (istype(inum, extent_protocol::T_FILE)) {
        printf("isfile: %llu is a file\n", inum);
        return true;
    }

    return false;
}

bool yfs_client::isdir_p(inum inum)
{
    if (istype(inum, extent_protocol::T_DIR)) {
        printf("isdir: %llu is a dir\n", inum);
        return true;
    }

    return false;
}

bool yfs_client::issymlink_p(inum inum)
{
    if (istype(inum, extent_protocol::T_SYMLINK)) {
        printf("issymlink: %llu is a symlink\n", inum);
        return true;
    }

    return false;
}

bool yfs_client::isfile(inum inum)
{
    bool r;

    LCK_RPC(lc->acquire(inum), false);
    r = isfile_p(inum);
    LCK_RPC(lc->release(inum), false);

    return r;
}

bool yfs_client::isdir(inum inum)
{
    bool r;

    LCK_RPC(lc->acquire(inum), false);
    r = isdir_p(inum);
    LCK_RPC(lc->release(inum), false);

    return r;
}

bool yfs_client::issymlink(inum inum)
{
    bool r;

    LCK_RPC(lc->acquire(inum), false);
    r = issymlink_p(inum);
    LCK_RPC(lc->release(inum), false);

    return r;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    if (!inum_valid(inum))
        return IOERR;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;

    LCK_RPC(lc->acquire(inum), IOERR);
    EXT_RPC(ec->getattr(inum, a));

    if (a.type != extent_protocol::T_FILE) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    LCK_RPC(lc->release(inum), IOERR);
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    if (!inum_valid(inum))
        return IOERR;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;

    LCK_RPC(lc->acquire(inum), IOERR);
    EXT_RPC(ec->getattr(inum, a));

    if (a.type != extent_protocol::T_DIR) {
        r = IOERR;
        goto release;
    }

    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    LCK_RPC(lc->release(inum), IOERR);
    return r;
}

int yfs_client::getsymlink(inum inum, fileinfo &fin)
{
    int r = OK;

    if (!inum_valid(inum))
        return IOERR;

    printf("getsymlink %016llx\n", inum);
    extent_protocol::attr a;

    LCK_RPC(lc->acquire(inum), IOERR);
    EXT_RPC(ec->getattr(inum, a));

    if (a.type != extent_protocol::T_SYMLINK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getsymlink %016llx -> sz %llu\n", inum, fin.size);

release:
    LCK_RPC(lc->release(inum), IOERR);
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

    LCK_RPC(lc->acquire(ino), IOERR);

    // Get current content.
    EXT_RPC(ec->get(ino, content));

    csize = content.length();

    // Write new content.
    EXT_RPC(ec->put(ino, size > csize ? (content + std::string(size - csize, '\0')) : content.substr(0, size)));

release:
    LCK_RPC(lc->release(ino), IOERR);
    return r;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r;

    LCK_RPC(lc->acquire(parent), IOERR);
    r = createitem(parent, name, mode, ino_out, extent_protocol::T_FILE);
    LCK_RPC(lc->release(parent), IOERR);

    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r;

    LCK_RPC(lc->acquire(parent), IOERR);
    r = createitem(parent, name, mode, ino_out, extent_protocol::T_DIR);
    LCK_RPC(lc->release(parent), IOERR);

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

    found = false;

    if (!name)
        return r;

    std::string fname = std::string(name);

    if (!filename_valid(fname))
        return r;

    std::list<dirent> itemlist;

    LCK_RPC(lc->acquire(parent), IOERR);
    if (!isdir_p(parent)) {
        r = IOERR;
        goto release;
    }

    r = readdir_p(parent, itemlist);

    if (r != OK)
        goto release;

    for (std::list<dirent>::iterator it = itemlist.begin(); it != itemlist.end(); ++it) {
        if (it->name == fname) {
            found = true;
            ino_out = it->inum;
            break;
        }
    }

release:
    LCK_RPC(lc->release(parent), IOERR);
    return r;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r;

    LCK_RPC(lc->acquire(dir), IOERR);
    r = readdir_p(dir, list);
    LCK_RPC(lc->release(dir), IOERR);

    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */

    std::string content;

    LCK_RPC(lc->acquire(ino), IOERR);
    if (!isfile_p(ino)) {
        data.clear();
        r = IOERR;
        goto release;
    }

    EXT_RPC(ec->get(ino, content));
    data = content.substr(off, size);

release:
    LCK_RPC(lc->release(ino), IOERR);
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

    size_t newsize;
    std::string content, newcontent;
    char *buf;

    // Check input parameters.
    if (!data)
        return r;

    LCK_RPC(lc->acquire(ino), IOERR);
    if (!isfile_p(ino)) {
        r = IOERR;
        goto release;
    }

    // Read original content.
    EXT_RPC(ec->get(ino, content));

    newsize = maxsize2(content.length(), off + size);

    buf = (char*)calloc(newsize, 1); // Buffer already initialized to '\0's.
    if (!buf) {
        printf("Error: calloc failed.\n");
        exit(-1);
    }

    // Construct new content.
    memcpy(buf, content.c_str(), content.length());
    memcpy(buf + off, data, size);
    newcontent = std::string(buf, newsize); // We have to specify size, or the construction may stop at '\0'!
    free(buf);

    // Write back.
    EXT_RPC(ec->put(ino, newcontent));
    bytes_written = size;

release:
    LCK_RPC(lc->release(ino), IOERR);
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

    std::list<dirent> itemlist;
    std::list<dirent>::iterator it;
    inum delinum;

    // Check input parameters.
    if (!name)
        return NOENT;

    std::string fname = std::string(name);

    if (!filename_valid(fname))
        return NOENT;

    LCK_RPC(lc->acquire(parent), IOERR);
    if (!isdir_p(parent)) {
        r = IOERR;
        goto release;
    }

    // Read the directory entries.
    r = readdir_p(parent, itemlist);
    if (r != OK)
        goto release;

    // Find position of the name.
    for (it = itemlist.begin(); it != itemlist.end(); ++it)
        if (it->name == fname)
            break;

    if (it == itemlist.end()) { // Name not found.
        r = NOENT;
        goto release;
    }

    delinum = it->inum;

    if (lc->acquire(delinum) != lock_protocol::OK) {
        r = IOERR;
        goto release;
    }

    if (!isfile_p(delinum) && !issymlink_p(delinum)) { // Not a (regular) file or a symlink, cannot unlink.
        r = IOERR;
    } else {
        // Remove the entry from the directory and write back.
        itemlist.erase(it);
        r = writedir(parent, itemlist);
        if (r == OK) {
            // Remove the inode.
            if (ec->remove(delinum) != extent_protocol::OK)
                r = IOERR;
        }
    }

    if (lc->release(delinum) != lock_protocol::OK) {
        r = IOERR;
        goto release;
    }

release:
    LCK_RPC(lc->release(parent), IOERR);
    return r;
}

int yfs_client::symlink(inum parent, const char *name, const char *target, inum &ino_out)
{
    int r = OK;

    // Check input parameters.
    if (!target || !strlen(target))
        return IOERR;

    LCK_RPC(lc->acquire(parent), IOERR);

    // Create a symlink type inode.
    r = createitem(parent, name, 0, ino_out, extent_protocol::T_SYMLINK);

    if (r != OK)
        goto release;

    if (lc->acquire(ino_out) != lock_protocol::OK) {
        r = IOERR;
        goto release;
    }

    // Write target path as its content.
    if (ec->put(ino_out, std::string(target)) != extent_protocol::OK)
        r = IOERR;

    if (lc->release(ino_out) != lock_protocol::OK) {
        r = IOERR;
        goto release;
    }

release:
    LCK_RPC(lc->release(parent), IOERR);
    return r;
}

int yfs_client::readlink(inum ino, std::string &target)
{
    int r = OK;

    LCK_RPC(lc->acquire(ino), IOERR);
    if (!issymlink_p(ino)) {
        target.clear();
        r = IOERR;
        goto release;
    }

    EXT_RPC(ec->get(ino, target));

release:
    LCK_RPC(lc->release(ino), IOERR);
    return r;
}
