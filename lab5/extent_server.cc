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

char vc_logfile[] = "extent_version.log";

void extent_server::read_log_entries(std::vector<std::string> &log_entries)
{
    log_entries.clear();
    std::ifstream fin(vc_logfile, std::ios_base::binary);
    int cnt;
    fin.read((char*)&cnt, sizeof(int));
    if (!fin)
        return;
    char *buf = new char[DISK_SIZE];
    for (int i = 0; i < cnt; ++i) {
        fin.read(buf, DISK_SIZE);
        if (!fin)
            break;
        log_entries.push_back(std::string(buf, DISK_SIZE));
    }
    delete[] buf;
    fin.close();
}

void extent_server::write_log_entries(std::vector<std::string> &log_entries)
{
    std::ofstream fout(vc_logfile, std::ios_base::binary);
    int cnt = log_entries.size();
    fout.write((char*)&cnt, sizeof(int));
    for (int i = 0; i < cnt; ++i)
        fout.write(log_entries[i].c_str(), DISK_SIZE);
    fout.close();
}

extent_server::extent_server()
{
    im = new inode_manager();

    std::ofstream fout(vc_logfile, std::ios_base::app);
    fout.close();

    int unused;
    commit(0, unused);
}

extent_server::~extent_server()
{
    delete im;
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id)
{
    // alloc a new inode and return inum
    printf("extent_server: create inode\n");
    id = im->alloc_inode(type);
    im->uncommitted = true;

    return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
    id &= 0x7fffffff;

    const char *cbuf = buf.c_str();
    int size = buf.size();
    im->write_file(id, cbuf, size);
    im->uncommitted = true;

    return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
    printf("extent_server: get %lld\n", id);

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

    return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
    printf("extent_server: getattr %lld\n", id);

    id &= 0x7fffffff;

    extent_protocol::attr attr;
    memset(&attr, 0, sizeof(attr));
    im->getattr(id, attr);
    a = attr;

    return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
    printf("extent_server: remove %lld\n", id);

    id &= 0x7fffffff;
    im->remove_file(id);
    im->uncommitted = true;

    return extent_protocol::OK;
}

int extent_server::commit(uint32_t, int &)
{
    printf("extent_server: commit\n");

    std::vector<std::string> log_entries;
    read_log_entries(log_entries);
    log_entries.resize(++im->current_version);
    log_entries.push_back(std::string(im->get_disk_ptr(), DISK_SIZE));
    write_log_entries(log_entries);
    im->uncommitted = false;

    return extent_protocol::OK;
}

int extent_server::undo(uint32_t, int &)
{
    printf("extent_server: undo\n");

    std::vector<std::string> log_entries;
    read_log_entries(log_entries);

    if (im->uncommitted) {
        memcpy(im->get_disk_ptr(), log_entries[im->current_version].c_str(), DISK_SIZE);
        im->uncommitted = false;
    } else {
        if (im->current_version >= 1)
            memcpy(im->get_disk_ptr(), log_entries[--im->current_version].c_str(), DISK_SIZE);
    }

    return extent_protocol::OK;
}

int extent_server::redo(uint32_t, int &)
{
    printf("extent_server: redo\n");

    std::vector<std::string> log_entries;
    read_log_entries(log_entries);

    if (im->current_version + 1 < log_entries.size())
        memcpy(im->get_disk_ptr(), log_entries[++im->current_version].c_str(), DISK_SIZE);

    return extent_protocol::OK;
}
