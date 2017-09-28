// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "extent_protocol.h"

#define DISK_SIZE (1024 * 1024 * 16)
#define BLOCK_SIZE 512
#define BLOCK_NUM (DISK_SIZE / BLOCK_SIZE)

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

class disk {
private:
    unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

public:
    disk();
    void read_block(uint32_t id, char *buf);
    void write_block(uint32_t id, const char *buf);
};

// block layer -----------------------------------------

typedef struct superblock {
    uint32_t size;
    uint32_t nblocks;
    uint32_t ninodes;
} superblock_t;

class block_manager {
private:
    disk *d;
    std::map <uint32_t, int> using_blocks;
    int least_available_in_block(const char *block_buf);
    int find_available_slot();
    void mark_as_allocated(uint32_t id);
    void mark_as_allocated_batch(uint32_t to_id);
    void mark_as_free(uint32_t id);
public:
    block_manager();
    ~block_manager();
    struct superblock sb;
    uint32_t alloc_block();
    void free_block(uint32_t id);
    void read_block(uint32_t id, char *buf);
    void write_block(uint32_t id, const char *buf);
};

// inode layer -----------------------------------------

#define INODE_NUM 1024

// Inodes per block.
#define IPB 1
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
//#define IBLOCK(i, nblocks) ((nblocks)/BPB + (i)/IPB + 3) // Suspect wrong
#define IBLOCK(i, nblocks) ((nblocks) / BPB + (i) / IPB + 1)

// Bitmap bits per block
#define BPB (BLOCK_SIZE * 8)

// The number of blocks for bitmap
#define BITMAP_BLOCKS (BLOCK_NUM / BPB)

// Block containing bit for block b
#define BBLOCK(b) ((b) / BPB + 2)

// Direct/indirect blocks number
#define NDIRECT 32
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// Mathematically ceil(x / y)
#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))

typedef struct inode {
    //short type;
    unsigned int type;
    unsigned int size;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    blockid_t blocks[NDIRECT + 1]; // Data block addresses
} inode_t;

class inode_manager {
private:
    block_manager *bm;
    struct inode* get_inode(uint32_t inum);
    void put_inode(uint32_t inum, struct inode *ino);
    void get_blockids(const inode_t *ino, blockid_t *bids, int cnt);
    void set_blockids(inode_t *ino, const blockid_t *bids, int cnt);
public:
    inode_manager();
    ~inode_manager();
    uint32_t alloc_inode(uint32_t type);
    void free_inode(uint32_t inum);
    void read_file(uint32_t inum, char **buf, int *size);
    void write_file(uint32_t inum, const char *buf, int size);
    void remove_file(uint32_t inum);
    void getattr(uint32_t inum, extent_protocol::attr &a);
};

#endif
