#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>


/************************** Defining Constants for file system *******************/
#define NR_BLOCKS 1024          // should be divisible with 8 (byte length)
#define BLOCK_SIZE 1024
#define MAX_DIRECT_BLOCKS 12    // maximum blocks for file content
#define MAX_INODES 256          // should be divisible with 8 (byte length)
#define BYTE_LEN 8
#define MAX_FILE_NAME 32
#define MAX_FILES_IN_DIRECTORY 32
#define BLOCK_MAP_LEN NR_BLOCKS / BYTE_LEN
#define INODE_MAP_LEN MAX_INODES / BYTE_LEN
#define ROOT_INODE_INDEX 2
#define FILESYSTEM_NAME "filesystem.bin"    // represents "disk"

struct superblock {
    int total_blocks;
    int block_size;
    int inode_count;
    int free_blocks;    // number of the blocks that are free of writing
    int free_nodes;     // number of the nodes that are free of writing

    int inode_bitmap_start;     // position for starting reading from disk
    int block_bitmap_start;
    int inode_table_start;
    int data_blocks_start;
} sb;

struct bitmap{
    unsigned char block_map[BLOCK_MAP_LEN];
    unsigned char inode_map[INODE_MAP_LEN];
} bm;

struct inode {
    int file_size;                         // content size of memorised file
    int file_type;                         // 0 for file, 1 for directory
    int direct_blocks[MAX_DIRECT_BLOCKS];  // maximum nr of memory blocks that can be allocated for one file/directory
    int crtBLocks;
    int parent_inode_index;                // the inode of directory where s located current file/directory
} inodes[MAX_INODES];

typedef struct {
    int inode_index;
    char filename[MAX_FILE_NAME];
} directory_entry;      // an instance that represents a file/directory in current directory

typedef struct {
    directory_entry entries[MAX_FILES_IN_DIRECTORY];
    int count;      // current number of files/directories
} directory;

// fac un array de directoare 

unsigned char disk_buffer[NR_BLOCKS][BLOCK_SIZE] = {0};   // for easy access


/************************** All functions used *******************/
int file_is_empty(char*);
int superblock_init();
int filesystem_init();
int set_bit_to_value(unsigned char*, int, int, int);
int find_bit_value(unsigned char*, int, int);
int findFreeBlockIndex();
int create_directory(int, char *);
int extract_data(void *, int);
int find_free_inode();
int updateMemory(void*, int, int);


/*****************************************************************/

int main() {
    printf("Salut! Acesta este sistemul tau de fisiere!");

    filesystem_init();

}

/*****************************************************************/
// verify if a file (disk) contains data from a precedent writing
// return 1 if file is empty
int file_is_empty(char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);

    fclose(f);
    return (size == 0);
}


/*****************************************************************/
// this function copies data from file to "buffer" and initiates suberblock
// return 1 for success
int superblock_init() {
    FILE *f = fopen(FILESYSTEM_NAME, "rb");
    if (!f) {
        // daca fisierul nu exista creeaza l tu
        return 0;
    }
    
    if (!file_is_empty(FILESYSTEM_NAME)) {      // if disk doesn t contain data, default initialization 
        for (int i = 0; i < NR_BLOCKS; i++) {
            fread(disk_buffer[i], BLOCK_SIZE, 1, f);
        }
        memmove(disk_buffer[0], &sb, sizeof(sb));       // read all superblock from first block of memory
    } else {
        sb.total_blocks = NR_BLOCKS;
        sb.block_size = BLOCK_SIZE;
        sb.inode_count = MAX_INODES;
        sb.free_blocks = NR_BLOCKS - 1;
        sb.free_nodes = MAX_INODES - 1;
        sb.inode_bitmap_start = 1;
        sb.block_bitmap_start = 2;
        sb.inode_table_start = 3;
        sb.data_blocks_start = 3 + ceil(MAX_INODES * sizeof(struct inode) / BLOCK_SIZE);   // find nr of blocks after inode table
    }
    fclose(f);

    return 1;
}


/*****************************************************************/
// initiates filesystem: default initaiate & create root/read disk
int filesystem_init() {
    if (superblock_init()) {
        memmove(disk_buffer[sb.inode_bitmap_start], bm.inode_map, INODE_MAP_LEN);
        memmove(disk_buffer[sb.block_bitmap_start], bm.block_map, BLOCK_MAP_LEN);
        memmove(disk_buffer + sb.inode_table_start, inodes, sizeof(inodes));
    } else {
        // reset bitmap of blocks
        memset(bm.block_map, 0, sizeof(bm.block_map));
        // all this blocks are occupied by superblock, bitmaps, inode table
        for (int i = 0; i < sb.data_blocks_start; i++) {
            set_bit_to_value(bm.block_map, i, BLOCK_MAP_LEN, 1);
        }

        // reset bitmap of inodes
        memset(bm.inode_map, 0, sizeof(bm.inode_map));

        // create root directory
        inodes[ROOT_INODE_INDEX].file_size = 0;
        inodes[ROOT_INODE_INDEX].file_type = 1;
        inodes[ROOT_INODE_INDEX].parent_inode_index = 2;
        set_bit_to_value(bm.inode_map, ROOT_INODE_INDEX, sizeof(bm.inode_map), 1);

        directory root = {0};
        updateMemory(&root, sizeof(root), ROOT_INODE_INDEX);
    }
    return 1;
}

/*****************************************************************/
// set a specific bit to 0 or 1
// return 1 for success, 0 error
int set_bit_to_value(unsigned char *arr, int nr_bit, int arr_len_in_bytes, int value) {
    // verify if bit can be set
    if ((nr_bit < 0 || nr_bit >= arr_len_in_bytes * BYTE_LEN)|| 
    (value != 0 && value != 1)) {
        return 0;
    }

    int pos = nr_bit / BYTE_LEN;        // find position of byte
    nr_bit = nr_bit % BYTE_LEN;         // find position of bit within byte
    switch (value) {
        case 1:
            arr[pos] |= (1 << nr_bit);  // set to 1
        break;
        case 0:
            arr[pos] &= ~(1 << nr_bit); // set to 0
        break;
    }
    return 1;
}


/*****************************************************************/
int create_directory(int parent_inode, char *dir_name) {
    // extract data and make necessary verifications
    directory parent_dir;
    if (!extract_data(&parent_dir, parent_inode)) {
        perror("Eroare la extragerea datelor!\n");
        return 0;
    };

    if (parent_dir.count >= MAX_FILES_IN_DIRECTORY) {
        perror("Acest director este plin!\n");
        return 0;
    }

    if (sb.free_nodes == 0) {
        perror("Sistemul de fisiere este plin!\n");
        return 0;
    }

    // find inode for director
    int new_inode = find_free_inode();
    if (new_inode == -1) return 0;
    sb.free_nodes--;

    // update parent directory and new directory entry
    parent_dir.entries[parent_dir.count].inode_index = new_inode;
    strcpy(parent_dir.entries[parent_dir.count].filename, dir_name);
    parent_dir.count++;
    // memorise updated parent directory
    int ok = 0;
    updateMemory(&parent_dir, sizeof(parent_dir), parent_inode);        // aici e ceva gresit

    directory new_dir = {0};
    ok = updateMemory(&new_dir, sizeof(new_dir), new_inode);
    if (!ok) {
        // delete new director
        // return 0
    }
    // set new inode
    set_bit_to_value(bm.inode_map, new_inode, sizeof(bm.inode_map), 1);
    inodes[new_inode].crtBLocks = 0;
    inodes[new_inode].file_size = 0;
    inodes[new_inode].file_type = 1;
    inodes[new_inode].parent_inode_index = parent_inode;

    return 1;
}

/*****************************************************************/
// extract data from blocks to specific array with inode help
int extract_data(void *a, int inode_index) {
    // see if inode is valid
    if (find_bit_value(bm.inode_map, inode_index, sizeof(bm.inode_map)) == 0) {
        return 0;
    }

    // cast array
    unsigned char* arr = (unsigned char*)a;

    int size = inodes[inode_index].file_size;
    int crtBlocks = inodes[inode_index].crtBLocks;

    // extract data and modify given array
    for (int i = 0; i < crtBlocks; i++) {
        int copy_size = (size < BLOCK_SIZE) ? size : BLOCK_SIZE;
        memmove(arr, disk_buffer[inodes[inode_index].direct_blocks[i]], copy_size);
        arr += copy_size;
        size -= copy_size;
    }

    return 1;
}

/*****************************************************************/
// find value of a specific bit from an array
// return 0/1, or -1 for error
int find_bit_value(unsigned char* arr, int pos, int size) {
    // position should be valid
    if (pos < 0 && pos >= size * BYTE_LEN) return -1;

    int index = pos / BYTE_LEN;
    int offset = pos % BYTE_LEN;
    if ((arr[index] & (1 << offset)) == 0) {
        return 0;
    }
    return 1;
}


/*****************************************************************/
// if size of a file/directory is modified, update disk, inode, bitmaps
int updateMemory(void *a, int size, int inode_index) {
    unsigned char *arr = (unsigned char *)a;
    // calculate all blocks that we need
    int requiredBlocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (requiredBlocks > MAX_DIRECT_BLOCKS) {
        perror("Memoria alocata individual a atins maximul!\n");
        return 0;
    }

    if (sb.free_blocks < requiredBlocks - inodes[inode_index].crtBLocks) {
        perror("Nu mai exista memorie libera pe disc!\n");
        return 0;
    }

    // update data to the old blocks
    for (int i = 0; i < requiredBlocks && i < inodes[inode_index].crtBLocks; i++) {
        memmove(disk_buffer[inodes[inode_index].direct_blocks[i]], arr + BLOCK_SIZE * i, BLOCK_SIZE);
    }

    // if necessary, allocate new blocks
    for (int i = inodes[inode_index].crtBLocks; i < requiredBlocks; i++) {
        // looking for free block
        int free_block = findFreeBlockIndex();

        // write data to the newly allocated block
        memmove(disk_buffer[free_block], arr + BLOCK_SIZE * i, BLOCK_SIZE);
        set_bit_to_value(bm.block_map, free_block, sizeof(bm.block_map), 1);
        sb.free_blocks--;
        inodes[inode_index].direct_blocks[i] = free_block;
    }

    // if necessary, delete blocks
    for (int i = requiredBlocks; i < inodes[inode_index].crtBLocks; i++) {
        memset(disk_buffer[inodes[inode_index].direct_blocks[i]], 0, BLOCK_SIZE);
        set_bit_to_value(bm.block_map, inodes[inode_index].direct_blocks[i], sizeof(bm.block_map), 0);
        sb.free_blocks++;
    }

    inodes[inode_index].crtBLocks = requiredBlocks;
    inodes[inode_index].file_size = size;

    return 1;

}


/*****************************************************************/
// find index of free block
// return index or -1 for error
int findFreeBlockIndex() {
    // there s no free blocks anymore
    if (sb.free_blocks == 0) return -1;

    // find free block
    for (int i = 0; i < NR_BLOCKS; i++) {
        if (find_bit_value(bm.block_map, i, sizeof(bm.block_map)) == 0) {
            return i;
        }
    }

    return -1;
}


/*****************************************************************/
int find_free_inode() {
    if (sb.free_nodes == 0) return -1;

    for (int i = 0; i < MAX_INODES; i++) {
        if (find_bit_value(bm.inode_map, i, sizeof(bm.inode_map)) == 0) {
            return i;
        }
    }

    return -1;
}
