// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <vector>
#include <semaphore.h>
#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server {
protected:
#if 0
    typedef struct extent {
        std::string data;
        struct extent_protocol::attr attr;
    } extent_t;
    std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
    inode_manager *im;

    /* Following are variables and functions used to handle concurrency issues.
     * Inode operations (create, put, get, getattr, remove) are allowed to happen concurrently,
     * because their concurrency problems have already been handled in the previous lab.
     * However version control operations (commit, undo, redo) should not happen concurrently with any operation.
     *
     * This becomes a readers-writers problem. The inode operations can be regarded as readers, and
     * version control operations serves as writers. We choose to give writers a higher priority here,
     * because version control operations happen much less frequently than inode operations.
     */

    int readcount, writecount;
    sem_t rmutex, wmutex, readtry, resource;
    void reader_prologue();
    void reader_epilogue();
    void writer_prologue();
    void writer_epilogue();

public:
    extent_server();
    ~extent_server();

    int create(uint32_t type, extent_protocol::extentid_t &id);
    int put(extent_protocol::extentid_t id, std::string, int &);
    int get(extent_protocol::extentid_t id, std::string &);
    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
    int remove(extent_protocol::extentid_t id, int &);

    // Version Control Operations
    // The two parameters are not used. They only serve to satisfy the requirement of the RPC library.
    int commit(uint32_t, int &);
    int undo(uint32_t, int &);
    int redo(uint32_t, int &);
};

#endif
