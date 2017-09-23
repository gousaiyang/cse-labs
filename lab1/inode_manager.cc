#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
    bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf)
{
    /*
     *your lab1 code goes here.
     *if id is smaller than 0 or larger than BLOCK_NUM
     *or buf is null, just return.
     *put the content of target block into buf.
     *hint: use memcpy
    */

    if (id < 0 || id >= BLOCK_NUM || !buf)
        return;

    memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
    /*
     *your lab1 code goes here.
     *hint: just like read_block
    */

    if (id < 0 || id >= BLOCK_NUM || !buf)
        return;

    memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

int block_manager::least_available_in_block(const char *block_buf)
{
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        int revmap = ~block_buf[i];
        if (revmap) {
            for (int j = 0; j < 8; ++j)
                if (revmap & (1 << j))
                    return 8 * i + j;
        }
    }

    return -1;
}

int block_manager::find_available_slot()
{
    int pos = 1;
    int newid;
    char buf[BLOCK_SIZE];

    do {
        ++pos;
        read_block(pos, buf);
        newid = least_available_in_block(buf);
    } while (newid == -1 && pos < BITMAP_BLOCKS + 1);

    return newid == -1 ? -1 : (BPB * (pos - 2) + newid);
}

void block_manager::mark_as_allocated(uint32_t id)
{
    int pos = BBLOCK(id);
    int subid = id % BPB;
    char buf[BLOCK_SIZE];

    read_block(pos, buf);
    buf[subid / 8] |= 1 << (subid % 8);
    write_block(pos, buf);
}

void block_manager::mark_as_allocated_batch(uint32_t to_id)
{
    int last_pos = BBLOCK(to_id);
    int whole_bytes = to_id % BPB / 8;
    int remain_bits = to_id % BPB % 8;
    char buf[BLOCK_SIZE];

    for (int i = 0; i < BLOCK_SIZE; ++i)
        buf[i] = 0xff;

    for (int i = 2; i < last_pos; ++i)
        write_block(i, buf);
    
    read_block(last_pos, buf);

    for (int i = 0; i < whole_bytes; ++i)
        buf[i] = 0xff;

    for (int i = 0; i < remain_bits; ++i)
        buf[whole_bytes] |= 1 << (i % 8);

    write_block(last_pos, buf);
}

void block_manager::mark_as_free(uint32_t id)
{
    int pos = BBLOCK(id);
    int subid = id % BPB;
    char buf[BLOCK_SIZE];

    read_block(pos, buf);
    buf[subid / 8] &= ~(1 << (subid % 8));
    write_block(pos, buf);
}

// Allocate a free disk block.
blockid_t block_manager::alloc_block()
{
    /*
     * your lab1 code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.

     *hint: use macro IBLOCK and BBLOCK.
            use bit operation.
            remind yourself of the layout of disk.
     */

    int newid = find_available_slot();
    if (newid == -1) {
        printf("Error: no blocks avaliable!\n");
        exit(-1);
    }

    mark_as_allocated(newid);
    return newid;
}

void block_manager::free_block(uint32_t id)
{
    /*
     * your lab1 code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */

    if (id < 0 || id >= BLOCK_NUM)
        return;

    mark_as_free(id);
}

// The layout of disk should be like this:
// |<-boot->|<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

    mark_as_allocated_batch(2 + BITMAP_BLOCKS + INODE_NUM / IPB);
}

block_manager::~block_manager()
{
    delete d;
}

void block_manager::read_block(uint32_t id, char *buf)
{
    d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf)
{
    d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
    bm = new block_manager();
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
}

inode_manager::~inode_manager()
{
    delete bm;
}

/* Create a new file.
 * Return its inum. */
uint32_t inode_manager::alloc_inode(uint32_t type)
{
    /*
     * your lab1 code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     * the 1st is used for root_dir, see inode_manager::inode_manager().

     * if you get some heap memory, do not forget to free it.
     */

    int newinum = 0;
    int pos;
    char buf[BLOCK_SIZE];
    inode_t *ino;

    do {
        ++newinum;
        pos = IBLOCK(newinum, bm->sb.nblocks);
        bm->read_block(pos, buf);
        ino = (inode_t*)buf + (newinum - 1) % IPB;
    } while (ino->type && newinum < INODE_NUM);

    if (newinum == INODE_NUM) {
        printf("Error: no inode numbers avaliable!\n");
        exit(-1);
    }

    bzero(ino, sizeof(inode_t));
    ino->type = type;
    bm->write_block(pos, buf);

    return newinum;
}

void inode_manager::free_inode(uint32_t inum)
{
    /*
     * your lab1 code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     * do not forget to free memory if necessary.
     */

    if (inum < 1 || inum > INODE_NUM)
        return;

    char buf[BLOCK_SIZE];
    int pos = IBLOCK(inum, bm->sb.nblocks);

    bm->read_block(pos, buf);
    inode_t *ino = (inode_t*)buf + (inum - 1) % IPB;
    ino->type = 0;
    bm->write_block(pos, buf);
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* inode_manager::get_inode(uint32_t inum)
{
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    printf("\tim: get_inode %d\n", inum);

    if (inum < 1 || inum > INODE_NUM) {
        printf("\tim: inum out of range\n");
        return NULL;
    }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    // printf("%s:%d\n", __FILE__, __LINE__);

    ino_disk = (struct inode*)buf + (inum - 1) % IPB;
    if (ino_disk->type == 0) {
        printf("\tim: inode not exist\n");
        return NULL;
    }

    ino = (struct inode*)malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    printf("\tim: put_inode %d\n", inum);

    if (inum < 1 || inum > INODE_NUM) {
        printf("\tim: inum out of range\n");
        return;
    }

    if (ino == NULL)
        return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode*)buf + (inum - 1) % IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum.
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
    /*
     * your lab1 code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_out
     */
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
    /*
     * your lab1 code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf
     * is larger or smaller than the size of original inode.
     * you should free some blocks if necessary.
     */
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
    /*
     * your lab1 code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */
    
    inode_t* ino = get_inode(inum);
    if (!ino) {
        printf("\tim: bad inode\n");
        return;
    }

    a.type = ino->type;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;
    a.size = ino->size;

    free(ino);
}

void inode_manager::remove_file(uint32_t inum)
{
    /*
     * your lab1 code goes here
     * note: you need to consider about both the data block and inode of the file
     * do not forget to free memory if necessary.
     */
}
