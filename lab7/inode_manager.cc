#include <pthread.h>
#include "inode_manager.h"

void print_string_hex(const char* buf, int size) //###
{
    printf("---hex start---\n");
    for (int i = 0; i < size; ++i)
        printf("%02x ", (byte)buf[i]);

    printf("\n---hex end---\n");
}

/* Fill a byte (b0b1b2b3b4b5b6b7) with one of its bits in a given range (place 0 at other positions).
 * Example: bit_expand(0b00100000, 2, 0b00111100) = 0b00111100 */
inline byte bit_expand(byte c, int pos, int range)
{
    return (byte)(((int)c << (pos + 24) >> 31) & range);
}

// Extract a bit from a byte (b0b1b2b3b4b5b6b7) at the given position.
inline bool get_bit(byte c, int pos)
{
    return (bool)(c & (1 << (7 - pos)));
}

// Find majority of 5 bits.
inline bool voter_5(bool x1, bool x2, bool x3, bool x4, bool x5)
{
    return x1 + x2 + x3 + x4 + x5 >= 3;
}

// Construct a byte (b0b1b2b3b4b5b6b7) from 8 bits.
inline byte construct_byte(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7)
{
    return (byte)(b0 << 7 | b1 << 6 | b2 << 5 | b3 << 4 | b4 << 3 | b5 << 2 | b6 << 1 | b7);
}

/* For every byte b0b1b2b3b4b5b6b7, encoded result is 5 bytes as follows:
 * b0b0b0b0b0b1b1b1 b1b1b2b2b2b2b2b3 b3b3b3b3b4b4b4b4 b4b5b5b5b5b5b6b6 b6b6b6b7b7b7b7b7 */
std::string encode_data(const std::string &data)
{
    std::string result;
    int len = data.length();

    for (int i = 0; i < len; ++i) {
        // printf(">>> inode_manager::encode_data: processing byte %02x\n", (byte)data[i]); //###
        result.push_back(bit_expand(data[i], 0, 0xf8) | bit_expand(data[i], 1, 0x7));
        result.push_back(bit_expand(data[i], 1, 0xc0) | bit_expand(data[i], 2, 0x3e) | bit_expand(data[i], 3, 0x1));
        result.push_back(bit_expand(data[i], 3, 0xf0) | bit_expand(data[i], 4, 0xf));
        result.push_back(bit_expand(data[i], 4, 0x80) | bit_expand(data[i], 5, 0x7c) | bit_expand(data[i], 6, 0x3));
        result.push_back(bit_expand(data[i], 6, 0xe0) | bit_expand(data[i], 7, 0x1f));
        // printf(">>> inode_manager::encode_data: partial encoded bytes %02x %02x %02x %02x %02x\n",
        //     (byte)result[result.length()-5],
        //     (byte)result[result.length()-4],
        //     (byte)result[result.length()-3],
        //     (byte)result[result.length()-4],
        //     (byte)result[result.length()-1]); //###
    }

    return result;
}

std::string decode_data(const std::string &data)
{
    int len = data.length();
    if (len % 5) {
        printf("Error: encoded data size should be a multiple of 5");
        exit(-1);
        return std::string();
    }

    int parts = len / 5;
    std::string result;
    for (int i = 0; i < parts; ++i) {
        int byte0 = data[i * 5];
        int byte1 = data[i * 5 + 1];
        int byte2 = data[i * 5 + 2];
        int byte3 = data[i * 5 + 3];
        int byte4 = data[i * 5 + 4];
        bool bit0 = voter_5(get_bit(byte0, 0), get_bit(byte0, 1), get_bit(byte0, 2), get_bit(byte0, 3), get_bit(byte0, 4));
        bool bit1 = voter_5(get_bit(byte0, 5), get_bit(byte0, 6), get_bit(byte0, 7), get_bit(byte1, 0), get_bit(byte1, 1));
        bool bit2 = voter_5(get_bit(byte1, 2), get_bit(byte1, 3), get_bit(byte1, 4), get_bit(byte1, 5), get_bit(byte1, 6));
        bool bit3 = voter_5(get_bit(byte1, 7), get_bit(byte2, 0), get_bit(byte2, 1), get_bit(byte2, 2), get_bit(byte2, 3));
        bool bit4 = voter_5(get_bit(byte2, 4), get_bit(byte2, 5), get_bit(byte2, 6), get_bit(byte2, 7), get_bit(byte3, 0));
        bool bit5 = voter_5(get_bit(byte3, 1), get_bit(byte3, 2), get_bit(byte3, 3), get_bit(byte3, 4), get_bit(byte3, 5));
        bool bit6 = voter_5(get_bit(byte3, 6), get_bit(byte3, 7), get_bit(byte4, 0), get_bit(byte4, 1), get_bit(byte4, 2));
        bool bit7 = voter_5(get_bit(byte4, 3), get_bit(byte4, 4), get_bit(byte4, 5), get_bit(byte4, 6), get_bit(byte4, 7));
        result.push_back(construct_byte(bit0, bit1, bit2, bit3, bit4, bit5, bit6, bit7));
    }

    return result;
}

// disk layer -----------------------------------------

disk::disk()
{
    pthread_t id;
    int ret;
    bzero(blocks, sizeof(blocks));

    ret = pthread_create(&id, NULL, test_daemon, (void*)blocks);
    if (ret != 0)
        printf("FILE %s line %d:Create pthread error\n", __FILE__, __LINE__);
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

char* block_manager::get_disk_ptr()
{
    return (char*)d->blocks;
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

    assert(pthread_mutex_lock(&alloc_block_mutex) == 0);

    int newid = find_available_slot();
    if (newid == -1) {
        printf("Error: no blocks avaliable!\n");
        exit(-1);
    }

    mark_as_allocated(newid);

    assert(pthread_mutex_unlock(&alloc_block_mutex) == 0);

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

    assert(pthread_mutex_init(&alloc_block_mutex, NULL) == 0);

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
    assert(pthread_mutex_destroy(&alloc_block_mutex) == 0);
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
    uncommitted = false;
    current_version = -1;
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

char* inode_manager::get_disk_ptr()
{
    return bm->get_disk_ptr();
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

    // Handle inode with no content.
    if (ino->size == 0) {
        *size = 0;
        *buf_out = NULL;
        ino->atime = (unsigned int)time(NULL);
        put_inode(inum, ino);
        free(ino);
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

    int encoded_size = ENCODED_SIZE(ino->size);
    int whole_blocks = encoded_size / BLOCK_SIZE;
    int total_blocks = CEIL_DIV(encoded_size, BLOCK_SIZE);
    int last_bytes = encoded_size % BLOCK_SIZE;
    char *encoded_buf = (char*)malloc(encoded_size);

    // Get block ids of the inode.
    get_blockids(ino, read_blockids, total_blocks);

    // Read data and transfer to returned data pointer.
    for (int i = 0; i < whole_blocks; ++i)
        bm->read_block(read_blockids[i], encoded_buf + i * BLOCK_SIZE);

    if (last_bytes) {
        bm->read_block(read_blockids[whole_blocks], buf);
        memcpy(encoded_buf + whole_blocks * BLOCK_SIZE, buf, last_bytes);
    }

    std::string decoded_data = decode_data(std::string(encoded_buf, encoded_size));
    // printf(">>> In read file: inum = %u, encoded data = (%d) %s\n", inum, encoded_size, encoded_buf); //###
    printf(">>> In read file: inum = %u, encoded data size = (%d)\n", inum, encoded_size); //###
    // print_string_hex(encoded_buf, encoded_size); // ###
    // printf(">>> In read file: inum = %u, decoded data = (%d) %s\n", inum, (int)decoded_data.length(), decoded_data.c_str()); //###
    printf(">>> In read file: inum = %u, decoded data size = (%d)\n", inum, (int)decoded_data.length()); //###
    // print_string_hex(decoded_data.c_str(), decoded_data.length()); // ###
    free(encoded_buf);
    memcpy(*buf_out, decoded_data.c_str(), decoded_data.length());

    // Set atime of inode.
    ino->atime = (unsigned int)time(NULL);
    put_inode(inum, ino);

    write_file(inum, decoded_data.c_str(), decoded_data.length(), false);

    // Free memory allocated by get_inode().
    free(ino);
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size, bool set_timestamps /*= true*/)
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
    int old_block_num = CEIL_DIV(ENCODED_SIZE(ino->size), BLOCK_SIZE);
    get_blockids(ino, new_blockids, old_block_num);

    // Adjust block ids.
    int new_encoded_size = ENCODED_SIZE(size);
    int new_block_num = CEIL_DIV(new_encoded_size, BLOCK_SIZE);
    if (new_block_num > MAXFILE) {
        printf("Error: file too large");
        exit(-1);
    }
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
    int whole_blocks = new_encoded_size / BLOCK_SIZE;
    int last_bytes = new_encoded_size % BLOCK_SIZE;
    std::string encoded_string = encode_data(std::string(buf, size));
    const char *encoded_buf = encoded_string.c_str();
    // printf(">>> In write file: inum = %u, original data = (%d) %s\n", inum, size, buf); //###
    printf(">>> In write file: inum = %u, original data size = (%d)\n", inum, size); //###
    // print_string_hex(buf, size); //###
    // printf(">>> In write file: inum = %u, encoded data = (%d) %s\n", inum, encoded_string.length(), encoded_buf); //###
    printf(">>> In write file: inum = %u, encoded data size = (%d)\n", inum, encoded_string.length()); //###
    // print_string_hex(encoded_buf, encoded_string.length()); //###

    for (int i = 0; i < whole_blocks; ++i)
        bm->write_block(new_blockids[i], encoded_buf + i * BLOCK_SIZE);

    if (last_bytes) {
        memcpy(rest_buf, encoded_buf + whole_blocks * BLOCK_SIZE, last_bytes);
        bm->write_block(new_blockids[whole_blocks], rest_buf);
    }

    // Set new block ids, new size and mtime to inode.
    set_blockids(ino, new_blockids, new_block_num);
    ino->size = size;
    if (set_timestamps) {
        ino->mtime = (unsigned int)time(NULL);
        ino->ctime = (unsigned int)time(NULL);
    }
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

    int total_blocks = CEIL_DIV(ENCODED_SIZE(ino->size), BLOCK_SIZE);
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
