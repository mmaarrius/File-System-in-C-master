#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>


/************************** Defining Constants for file system *******************/
#define NR_BLOCKS 1024
#define BLOCK_SIZE 1024
#define MAX_DIRECT_BLOCKS 12    // maximum blocks for file content
#define MAX_INODES 256
#define BYTE_LEN 8
#define MAX_FILE_NAME 32
#define MAX_FILES_IN_DIRECTORY 32
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
} sb = {
    .total_blocks = NR_BLOCKS,
    .block_size = BLOCK_SIZE,
    .inode_count = MAX_INODES,
    .free_blocks = NR_BLOCKS - 1,
    .free_nodes = MAX_INODES - 1,
    .inode_bitmap_start = 1,
    .block_bitmap_start = 2,
    .inode_table_start = 3,
    .data_blocks_start = 3 + ceil(MAX_INODES * sizeof(struct inode) / BLOCK_SIZE)   // find nr of blocks after inode table
};

struct bitmap{
    unsigned char block_map[NR_BLOCKS / BYTE_LEN];
    unsigned char inode_map[MAX_INODES / BYTE_LEN];
} bm;

struct inode {
    int id;                                // unique id for every inode
    int file_size;                         // content size of memorised file
    int file_type;                         // if it s directory of simple file
    int direct_blocks[MAX_DIRECT_BLOCKS];  // maximim nr of memory blocks that can be allocated for one file
    int parent_inode;                      // the inode of directory where s located current file/directory
} inodes[MAX_INODES];

typedef struct {
    int inode_index;
    char filename[MAX_FILE_NAME];
} directory_entry;      // an instance that represents a file/directory in current directory

typedef struct {
    directory_entry entries[MAX_FILES_IN_DIRECTORY];
    int count;      // current number of files/directories
} directory;

unsigned char disk_buffer[NR_BLOCKS][BLOCK_SIZE] = {0};   // for easy access


/************************** All functions used *******************/
int file_is_empty(char*);
int superblock_init();


/*****************************************************************/

int main() {
    printf("Salut! Acesta este sistemul tau de fisiere!");

    superblock_init();
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
// this function copies data from file to "buffer"
// return 1 for success
int superblock_init() {
    FILE *f = fopen(FILESYSTEM_NAME, "rb");
    if (!f) {
        return 0;
    }
    
    if (!file_is_empty(FILESYSTEM_NAME)) {      // if disk doesn t contain data, default initialization 
        for (int i = 0; i < NR_BLOCKS; i++) {
            fread(disk_buffer[i], BLOCK_SIZE, 1, f);
        }
    }
    fclose(f);
    return 1;
}

int filesystem_init() {
    
}

