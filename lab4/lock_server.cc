// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server()
{
    VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
}

lock_server::~lock_server()
{
    VERIFY(pthread_mutex_destroy(&mutex) == 0);
}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;

    printf("stat request from clt %d\n", clt);

    VERIFY(pthread_mutex_lock(&mutex) == 0);
    r = lock_table.find(lid) == lock_table.end() ? -1 : lock_table[lid].nacquire;
    VERIFY(pthread_mutex_unlock(&mutex) == 0);

    return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;

    // Your lab4 code goes here
    printf("acquire request from clt %d\n", clt);

    VERIFY(pthread_mutex_lock(&mutex) == 0);
    if (lock_table.find(lid) != lock_table.end()) {
        while (lock_table[lid].flag) {
            VERIFY(pthread_mutex_unlock(&mutex) == 0);
            VERIFY(pthread_mutex_lock(&mutex) == 0);
        }
        lock_table[lid].flag = true;
        lock_table[lid].holder = clt;
        lock_table[lid].nacquire++;
    } else {
        lock_table[lid] = lock_state(true, clt, 1);
    }
    VERIFY(pthread_mutex_unlock(&mutex) == 0);
    r = 0;

    return ret;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;

    // Your lab4 code goes here
    printf("release request from clt %d\n", clt);

    VERIFY(pthread_mutex_lock(&mutex) == 0);
    if (lock_table.find(lid) == lock_table.end() || !lock_table[lid].flag || lock_table[lid].holder != clt) {
        r = -1;
    } else {
        lock_table[lid].flag = false;
        r = 0;
    }
    VERIFY(pthread_mutex_unlock(&mutex) == 0);

    return ret;
}
