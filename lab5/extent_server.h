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
    int commit(uint32_t, int &);
    int undo(uint32_t, int &);
    int redo(uint32_t, int &);
};

#endif
