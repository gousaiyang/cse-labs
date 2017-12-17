// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include "extent_protocol.h"

#define DISK_SIZE (1024 * 1024 * 32)
#define BLOCK_SIZE 1024
#define BLOCK_NUM (DISK_SIZE / BLOCK_SIZE)

typedef uint32_t blockid_t;

// redundant encode and decode for fault tolerance

#define ENCODE_FACTOR 4
#define ENCODED_SIZE(x) ((x) * ENCODE_FACTOR)
#define ENCODE_EXTRA_SIZE(x) ((x) * (ENCODE_FACTOR - 1))
typedef unsigned char byte;
inline bool get_bit(byte c, int pos);
inline bool voter_3(bool x1, bool x2, bool x3);
inline byte construct_byte(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7);
inline byte encode2to8(bool b0, bool b1);
inline void decode2to8(byte data, bool &b0, bool &b1);
std::string encode_data(const std::string &data);
std::string decode_data(const std::string &data);

// disk layer -----------------------------------------

class disk {
    friend class block_manager;
    friend class inode_manager;
    friend class extent_server;
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
    friend class inode_manager;
    friend class extent_server;
private:
    disk *d;
    std::map <uint32_t, int> using_blocks;
    pthread_mutex_t block_manager_mutex;
    void encode_bitmap_all();
    void decode_bitmap_all();
    void encode_bitmap(uint32_t bblock);
    void decode_bitmap(uint32_t bblock);
    int least_available_in_block(const char *block_buf);
    int find_available_slot();
    void mark_as_allocated(uint32_t id);
    void mark_as_allocated_batch(uint32_t to_id);
    void mark_as_free(uint32_t id);
    char* get_disk_ptr();
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

// Mathematically ceil(x / y)
#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))

#define INODE_NUM 1024

// Inodes per block.
#define IPB 1
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
//#define IBLOCK(i, nblocks) ((nblocks)/BPB + (i)/IPB + 3) // Suspect wrong
#define IBLOCK(i, nblocks) ((nblocks) / BPB + (i) / IPB + 1)

// The number of blocks for inode table
#define INODE_TABLE_BLOCKS (CEIL_DIV(INODE_NUM, IPB))

// Bitmap bits per block
#define BPB (BLOCK_SIZE * 8)

// The number of blocks for bitmap
#define BITMAP_BLOCKS (BLOCK_NUM / BPB)

// Block containing bit for block b
#define BBLOCK(b) ((b) / BPB + 2)

/* The number of reserved blocks, including:
 * boot block and super block
 * bitmap blocks and blocks for inode table (along with blocks for their fault tolerance encoding).
 */
#define RESERVED_BLOCKS_NUM (2 + ENCODED_SIZE(BITMAP_BLOCKS + INODE_TABLE_BLOCKS))

// Direct/indirect blocks number
#define NDIRECT 100
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

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
    friend class extent_server;
private:
    block_manager *bm;
    bool uncommitted;
    int current_version;
    pthread_mutex_t inode_manager_mutex;
    void encode_inode_table_all();
    void decode_inode_table_all();
    void encode_inode_table(uint32_t inum);
    void decode_inode_table(uint32_t inum);
    struct inode* get_inode(uint32_t inum);
    void put_inode(uint32_t inum, struct inode *ino);
    void get_blockids(const inode_t *ino, blockid_t *bids, int cnt);
    void set_blockids(inode_t *ino, const blockid_t *bids, int cnt);
    char* get_disk_ptr();
public:
    inode_manager();
    ~inode_manager();
    uint32_t alloc_inode(uint32_t type);
    void free_inode(uint32_t inum);
    void read_file(uint32_t inum, char **buf, int *size);
    void write_file(uint32_t inum, const char *buf, int size, bool set_timestamps = true);
    void remove_file(uint32_t inum);
    void getattr(uint32_t inum, extent_protocol::attr &a);
};

void* test_daemon(void* arg);
#endif
