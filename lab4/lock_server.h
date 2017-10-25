// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include <pthread.h>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_state {
public:
    bool flag;
    int holder;
    int nacquire;
    lock_state() {}
    lock_state(bool f, int h, int n): flag(f), holder(h), nacquire(n) {}
};

class lock_server {
protected:
    std::map<lock_protocol::lockid_t, lock_state> lock_table;
    pthread_mutex_t mutex;

public:
    lock_server();
    ~lock_server();
    lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
    lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
    lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif
