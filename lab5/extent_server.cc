// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

char vc_logfile[] = "extent_version.log";

void Sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (sem_init(sem, pshared, value) < 0) {
        printf("sem_init: failed: %s\n", strerror(errno));
        exit(-1);
    }
}

void Sem_destroy(sem_t *sem)
{
    if (sem_destroy(sem) < 0) {
        printf("sem_destroy: failed: %s\n", strerror(errno));
        exit(-1);
    }
}

void P(sem_t *sem)
{
    if (sem_wait(sem) < 0) {
        printf("sem_wait: failed: %s\n", strerror(errno));
        exit(-1);
    }
}

void V(sem_t *sem)
{
    if (sem_post(sem) < 0) {
        printf("sem_post: failed: %s\n", strerror(errno));
        exit(-1);
    }
}

void extent_server::reader_prologue()
{
    P(&readtry);
    P(&rmutex);
    readcount++;
    if (readcount == 1)
        P(&resource);
    V(&rmutex);
    V(&readtry);

}

void extent_server::reader_epilogue()
{
    P(&rmutex);
    readcount--;
    if (readcount == 0)
        V(&resource);
    V(&rmutex);
}

void extent_server::writer_prologue()
{
    P(&wmutex);
    writecount++;
    if (writecount == 1)
        P(&readtry);
    V(&wmutex);
    P(&resource);
}

void extent_server::writer_epilogue()
{
    V(&resource);
    P(&wmutex);
    writecount--;
    if (writecount == 0)
        V(&readtry);
    V(&wmutex);
}

extent_server::extent_server()
{
    im = new inode_manager();

    std::fstream f(vc_logfile, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    if (!f.is_open()) {
        printf("Error: cannot open version log file\n");
        exit(1);
    }

    readcount = 0;
    writecount = 0;

    Sem_init(&rmutex, 0, 1);
    Sem_init(&wmutex, 0, 1);
    Sem_init(&readtry, 0, 1);
    Sem_init(&resource, 0, 1);

    int cnt = 0;
    f.read((char*)&cnt, sizeof(int));
    if (f.eof())
        f.write((char*)&cnt, sizeof(int));
    f.close();

    int unused;
    commit(0, unused);
}

extent_server::~extent_server()
{
    Sem_destroy(&rmutex);
    Sem_destroy(&wmutex);
    Sem_destroy(&readtry);
    Sem_destroy(&resource);

    delete im;
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id)
{
    // alloc a new inode and return inum
    printf("extent_server: create inode\n");

    reader_prologue();

    id = im->alloc_inode(type);
    im->uncommitted = true;

    reader_epilogue();

    return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
    reader_prologue();

    id &= 0x7fffffff;

    const char *cbuf = buf.c_str();
    int size = buf.size();
    im->write_file(id, cbuf, size);
    im->uncommitted = true;

    reader_epilogue();

    return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
    printf("extent_server: get %lld\n", id);

    reader_prologue();

    id &= 0x7fffffff;

    int size = 0;
    char *cbuf = NULL;

    im->read_file(id, &cbuf, &size);
    if (size == 0)
        buf = "";
    else {
        buf.assign(cbuf, size);
        free(cbuf);
    }

    reader_epilogue();

    return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
    printf("extent_server: getattr %lld\n", id);

    reader_prologue();

    id &= 0x7fffffff;

    extent_protocol::attr attr;
    memset(&attr, 0, sizeof(attr));
    im->getattr(id, attr);
    a = attr;

    reader_epilogue();

    return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
    printf("extent_server: remove %lld\n", id);

    reader_prologue();

    id &= 0x7fffffff;
    im->remove_file(id);
    im->uncommitted = true;

    reader_epilogue();

    return extent_protocol::OK;
}

int extent_server::commit(uint32_t, int &)
{
    printf("extent_server: commit\n");

    writer_prologue();

    std::fstream f(vc_logfile, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    int cnt;
    f.read((char*)&cnt, sizeof(int));
    f.seekp(++im->current_version * DISK_SIZE, std::ios_base::cur);
    f.write(im->get_disk_ptr(), DISK_SIZE);
    f.seekp(0);
    cnt = im->current_version + 1;
    f.write((char*)&cnt, sizeof(int));
    f.close();
    im->uncommitted = false;

    writer_epilogue();

    return extent_protocol::OK;
}

int extent_server::undo(uint32_t, int &)
{
    printf("extent_server: undo\n");

    writer_prologue();

    std::ifstream fin(vc_logfile, std::ios_base::binary);

    if (im->uncommitted) {
        fin.seekg(sizeof(int) + im->current_version * DISK_SIZE);
        fin.read(im->get_disk_ptr(), DISK_SIZE);
        im->uncommitted = false;
    } else {
        if (im->current_version >= 1) {
            fin.seekg(sizeof(int) + --im->current_version * DISK_SIZE);
            fin.read(im->get_disk_ptr(), DISK_SIZE);
        }
    }

    writer_epilogue();

    fin.close();

    return extent_protocol::OK;
}

int extent_server::redo(uint32_t, int &)
{
    printf("extent_server: redo\n");

    writer_prologue();

    std::ifstream fin(vc_logfile, std::ios_base::binary);
    int cnt;
    fin.read((char*)&cnt, sizeof(int));

    if (im->current_version + 1 < cnt) {
        fin.seekg(++im->current_version * DISK_SIZE, std::ios_base::cur);
        fin.read(im->get_disk_ptr(), DISK_SIZE);
        im->uncommitted = false;
    }

    fin.close();

    writer_epilogue();

    return extent_protocol::OK;
}
