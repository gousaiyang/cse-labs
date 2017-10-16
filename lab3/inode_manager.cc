#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
    bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf)
{
    /*
     * your lab1 code goes here.
     * if id is smaller than 0 or larger than BLOCK_NUM
     * or buf is null, just return.
     * put the content of target block into buf.
     * hint: use memcpy
     */

    if (id < 0 || id >= BLOCK_NUM || !buf)
        return;

    memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
    /*
     * your lab1 code goes here.
     * hint: just like read_block
     */

    if (id < 0 || id >= BLOCK_NUM || !buf)
        return;

    memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

int block_manager::least_available_in_block(const char *block_buf) // Return lowest '0' position in a block.
{
    for (int i = 0; i < BLOCK_SIZE; ++i) { // Traverse every byte in the block.
        int revmap = ~block_buf[i];
        if (revmap) { // Has available slot in this byte.
            for (int j = 0; j < 8; ++j)
                if (revmap & (1 << j))
                    return 8 * i + j;
        }
    }

    return -1; // No available slot in this block.
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
    int pos = BBLOCK(id); // Locate the block.
    int subid = id % BPB; // Bit position inside the block.
    char buf[BLOCK_SIZE];

    read_block(pos, buf);
    buf[subid / 8] |= 1 << (subid % 8);
    write_block(pos, buf);
}

void block_manager::mark_as_allocated_batch(uint32_t to_id) // Mark blocks [0, to_id) as allocated.
{
    int last_pos = BBLOCK(to_id);
    int whole_bytes = to_id % BPB / 8;
    int remain_bits = to_id % BPB % 8;
    char buf[BLOCK_SIZE];

    memset(buf, 0xff, BLOCK_SIZE);

    // Mark whole blocks.
    for (int i = 2; i < last_pos; ++i)
        write_block(i, buf);
    
    read_block(last_pos, buf);

    // Mark whole bytes.
    memset(buf, 0xff, whole_bytes);

    // Mark Remaining bits.
    buf[whole_bytes] |= (1 << remain_bits) - 1;

    write_block(last_pos, buf);
}

void block_manager::mark_as_free(uint32_t id)
{
    int pos = BBLOCK(id); // Locate the block.
    int subid = id % BPB; // Bit position inside the block.
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

     * hint: use macro IBLOCK and BBLOCK.
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

    // Write super block.
    char buf[BLOCK_SIZE];
    bzero(buf, BLOCK_SIZE);
    memcpy(buf, &sb, sizeof(superblock_t));
    write_block(1, buf);

    // Mark boot block, super block, bitmap blocks and blocks for inode table as allocated.
    mark_as_allocated_batch(2 + BITMAP_BLOCKS + CEIL_DIV(INODE_NUM, IPB));
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

    // Find the least available inode number.
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

    // Initialize the inode.
    bzero(ino, sizeof(inode_t));
    ino->type = type;
    ino->size = 0;
    ino->atime = (unsigned int)time(NULL);
    ino->mtime = (unsigned int)time(NULL);
    ino->ctime = (unsigned int)time(NULL);
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
    ino->type = 0; // Set inode type to 0 to mark its number as free.
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

    if (!ino) {
        printf("Error: malloc failed\n");
        exit(-1);
    } 

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

void inode_manager::get_blockids(const inode_t *ino, blockid_t *bids, int cnt) // Get first cnt block ids from an inode.
{
    char buf[BLOCK_SIZE];

    if (cnt <= NDIRECT) { // No indirect blocks.
        memcpy(bids, ino->blocks, cnt * sizeof(blockid_t));
    } else { // Involves an indirect block. Get direct blocks and more blocks from the indirect block.
        memcpy(bids, ino->blocks, NDIRECT * sizeof(blockid_t));
        bm->read_block(ino->blocks[NDIRECT], buf);
        memcpy(bids + NDIRECT, buf, (cnt - NDIRECT) * sizeof(blockid_t));
    }
}

void inode_manager::set_blockids(inode_t *ino, const blockid_t *bids, int cnt) // Set cnt block ids to an inode.
{
    char buf[BLOCK_SIZE];

    if (cnt <= NDIRECT) { // No indirect blocks.
        memcpy(ino->blocks, bids, cnt * sizeof(blockid_t));
    } else { // Need an indirect block. Set direct blocks and more blocks to the indirect block.
        memcpy(ino->blocks, bids, NDIRECT * sizeof(blockid_t));
        memcpy(buf, bids + NDIRECT, (cnt - NDIRECT) * sizeof(blockid_t));
        bm->write_block(ino->blocks[NDIRECT], buf);
    }
}

#define MIN(a,b) ((a)<(b) ? (a) : (b)) // Seems unused.

/* Get all the data of a file by inum.
 * Return allocated data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
    /*
     * your lab1 code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_out
     */

    // Retrieve the corresponding inode.
    inode_t* ino = get_inode(inum);
    if (!ino) {
        printf("\tim: bad inode\n");
        *size = 0;
        return;
    }

    // Return inode size and set returned data pointer.
    *size = ino->size;
    *buf_out = (char*)malloc(ino->size);

    if (!*buf_out) {
        printf("Error: malloc failed\n");
        exit(-1);
    }

    char buf[BLOCK_SIZE];
    blockid_t read_blockids[MAXFILE];

    int whole_blocks = ino->size / BLOCK_SIZE;
    int total_blocks = CEIL_DIV(ino->size, BLOCK_SIZE);
    int last_bytes = ino->size % BLOCK_SIZE;

    // Get block ids of the inode.
    get_blockids(ino, read_blockids, total_blocks);

    // Read data and transfer to returned data pointer.
    for (int i = 0; i < whole_blocks; ++i)
        bm->read_block(read_blockids[i], *buf_out + i * BLOCK_SIZE);

    if (last_bytes) {
        bm->read_block(read_blockids[whole_blocks], buf);
        memcpy(*buf_out + whole_blocks * BLOCK_SIZE, buf, last_bytes);
    }

    // Set atime of inode.
    ino->atime = (unsigned int)time(NULL);
    put_inode(inum, ino);

    // Free memory allocated by get_inode().
    free(ino);
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

    if (!buf)
        return;

    // Retrieve the corresponding inode.
    inode_t* ino = get_inode(inum);
    if (!ino) {
        printf("\tim: bad inode\n");
        return;
    }

    blockid_t new_blockids[MAXFILE];
    char rest_buf[BLOCK_SIZE];

    // Get original block ids.
    int old_block_num = CEIL_DIV(ino->size, BLOCK_SIZE);
    get_blockids(ino, new_blockids, old_block_num);

    // Adjust block ids.
    int new_block_num = CEIL_DIV(size, BLOCK_SIZE);
    int diff_num;

    if (new_block_num > old_block_num) { // Need to allocated more blocks.
        diff_num = new_block_num - old_block_num;
        for (int i = 0; i < diff_num; ++i)
            new_blockids[old_block_num + i] = bm->alloc_block();

        if (old_block_num <= NDIRECT && new_block_num > NDIRECT) // Need to allocate the indirect block.
            ino->blocks[NDIRECT] = bm->alloc_block();

    } else if (new_block_num < old_block_num) { // We can free some blocks.
        diff_num = old_block_num - new_block_num;
        for (int i = 0; i < diff_num; ++i)
            bm->free_block(new_blockids[old_block_num - 1 - i]);

        if (old_block_num > NDIRECT && new_block_num <= NDIRECT) // We can free the indirect block.
            bm->free_block(ino->blocks[NDIRECT]);
    }

    // Write data to data blocks.
    int whole_blocks = size / BLOCK_SIZE;
    int last_bytes = size % BLOCK_SIZE;

    for (int i = 0; i < whole_blocks; ++i)
        bm->write_block(new_blockids[i], buf + i * BLOCK_SIZE);

    if (last_bytes) {
        memcpy(rest_buf, buf + whole_blocks * BLOCK_SIZE, last_bytes);
        bm->write_block(new_blockids[whole_blocks], rest_buf);
    }
    
    // Set new block ids, new size and mtime to inode.
    set_blockids(ino, new_blockids, new_block_num);
    ino->size = size;
    ino->mtime = (unsigned int)time(NULL);
    ino->ctime = (unsigned int)time(NULL);
    put_inode(inum, ino);

    // Free memory allocated by get_inode().
    free(ino);
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
    /*
     * your lab1 code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */
    
    // Retrieve the corresponding inode.
    inode_t* ino = get_inode(inum);
    if (!ino) {
        printf("\tim: bad inode\n");
        a.type = 0;
        return;
    }

    // Read attr from inode and return to a.
    a.type = ino->type;
    a.size = ino->size;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;

    // Free memory allocated by get_inode().
    free(ino);
}

void inode_manager::remove_file(uint32_t inum)
{
    /*
     * your lab1 code goes here
     * note: you need to consider about both the data block and inode of the file
     * do not forget to free memory if necessary.
     */

    // Retrieve the corresponding inode.
    inode_t* ino = get_inode(inum);
    if (!ino) {
        printf("\tim: bad inode\n");
        return;
    }

    // Get block ids of the inode.
    blockid_t remove_blockids[MAXFILE];

    int total_blocks = CEIL_DIV(ino->size, BLOCK_SIZE);
    get_blockids(ino, remove_blockids, total_blocks);

    // Free all the data blocks.
    for (int i = 0; i < total_blocks; ++i)
        bm->free_block(remove_blockids[i]);

    // Free the indirect block if present.
    if (total_blocks > NDIRECT)
        bm->free_block(ino->blocks[NDIRECT]);

    // Free the inode (mark inum as free).
    free_inode(inum);

    // Free memory allocated by get_inode().
    free(ino);
}
