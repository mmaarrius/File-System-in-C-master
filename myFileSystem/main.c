#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/************************** Defining Constants for file system *******************/

#define NR_BLOCKS 1024          // should be divisible with 8 (byte length)
#define BLOCK_SIZE 1024
#define MAX_DIRECT_BLOCKS 12    // maximum blocks for file content
#define MAX_CONTENT_IN_FILE MAX_DIRECT_BLOCKS * BLOCK_SIZE
#define MAX_INODES 256          // should be divisible with 8 (byte length)
#define BYTE_LEN 8
#define MAX_FILE_NAME 30
#define MAX_FILES_IN_DIRECTORY 30
#define BLOCK_MAP_LEN NR_BLOCKS / BYTE_LEN
#define INODE_MAP_LEN MAX_INODES / BYTE_LEN
#define ROOT_INODE_INDEX 2
#define LINESIZE 256
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
} inodes[MAX_INODES] = {0};

typedef struct {
    int inode_index;
    char filename[MAX_FILE_NAME];
} directory_entry;      // an instance that represents a file/directory in current directory

typedef struct {
    directory_entry entries[MAX_FILES_IN_DIRECTORY];
    int count;      // current number of files/directories
} directory;

unsigned char disk_buffer[NR_BLOCKS][BLOCK_SIZE] = {0};   // for easy access

int crtInode;
unsigned char *path;

/************************** functions *******************/

/* COMMAND ------------------------>  ACTION 
- mkdir <path/directory_name>         create new directory
- touch <path/file_name>              create new file
- echo <content> >/>> <path_to_file>  add content to a file
- rm <path_to_file>                   remove a file
- rmdir <path_to_directory>           remove an empty directory
- cd <path_to_directory>              change current location
- ls <path_to_directory>              list entries of directory
- pwd                                 show path until current directory
- cat <path_to_file>                  show content of file
*/

void execute_command(char **, int);
void create_dir_cmd(unsigned char *);
void list_cmd(int, char**);
void change_dir_cmd(unsigned char*);
void make_file_cmd(unsigned char*);
void rm_file_cmd(unsigned char*);
void rm_dir_cmd(unsigned char*);
void pwd_cmd();
void cat_cmd(unsigned char*);
void exit_cmd();

int file_is_empty(char*);
int superblock_init();
int filesystem_init();
int set_bit_to_value(unsigned char*, int, int, int);
int find_bit_value(unsigned char*, int, int);
int find_free_block();
int extract_data(void *, int);
int find_free_inode();
int update_memory(void*, int, int);
void parse(char *, int*, char **);
void print_path();
int find_inode_of_path(unsigned char *, int, int* ,unsigned char **);
int find_path_of_inode (int, unsigned char **);
int add_word_to_file(unsigned char *, int, int);


/*****************************************************************/

int main() {
    printf("Salut! Acesta este sistemul tau de fisiere!\n\n");

    filesystem_init();

    // will store stdin 
    char in[LINESIZE];
    char *argv[LINESIZE];
    int argc;

    // we start from root
    path = malloc(1);
    path[0] = '/';
    crtInode = ROOT_INODE_INDEX;

    print_path();
    while (fgets(in, sizeof(in), stdin)) {
        // clear rest of stdin after line size
        if (!strchr(in, '\n')) {
            while (getchar() != '\n');
        }

        // switch '\n' with '\0' from command
        in[strcspn(in, "\n")] = '\0';

        // break down the arguments
        parse(in, &argc, argv);

        execute_command(argv, argc);

        find_path_of_inode(crtInode, &path);
        print_path();
    }
}

/*****************************************************************/
// just filtrate commands from input
void execute_command(char **argv, int argc) {
    // make directory command
    if (!strcmp(argv[0], "mkdir")) {
        for (int i = 1; i < argc; i++)
            create_dir_cmd(argv[i]);
    }
    // list command
    else if (!strcmp(argv[0], "ls")) {
        list_cmd(argc, argv);
    }
    // change directory command
    else if (!strcmp(argv[0], "cd")) {
        if (argc == 2)
            change_dir_cmd(argv[1]);
        else 
            printf("Argumente incorecte.\n");
    }
    // create file command
    else if (!strcmp(argv[0], "touch")) {
        for (int i = 1; i < argc; i++)
            make_file_cmd(argv[i]);
    }
    // remove file command
    else if (!strcmp(argv[0], "rm")) {
        if (argc == 2) {
            rm_file_cmd(argv[1]);
        } else {
            printf("Argumente incorecte.\n");
        }
    }
    // remove directory command
    else if (!strcmp(argv[0], "rmdir")) {
        if (argc == 2) {
            rm_dir_cmd(argv[1]);
        } else {
            printf("Argumente incorecte.\n");
        }
    }
    // write content to file command
    else if (!strcmp(argv[0], "echo")) {
        int delete = 1;
        int sign_position = 0;
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], ">")) {
                delete = 1;
                sign_position = i;
            } else if (!strcmp(argv[i], ">>")) {
                delete = 0;
                sign_position = i;
            }
        }

        // verify if there s more paths after '>'/'>>'
        if (argc != (sign_position + 2)) {
            printf("Argumente incorecte.\n");
            return;
        }

        int file_inode = find_inode_of_path(argv[sign_position + 1], crtInode, NULL, NULL);
        if (file_inode < 0) {
            printf("Fisierul nu exista.\n");
            return;
        }

        // add space between words
        unsigned char space[] = " ";

        // add every word
        for (int i = 1; i < sign_position; i++) {
            if (!add_word_to_file(argv[i], file_inode, delete)) {
                return;
            }
            add_word_to_file(space, file_inode, 0);

            // next words from phrase won t delete last one s
            delete = 0;
        }
        printf("Continutul a fost adaugat.\n");
    }
    // show path until now command
    else if (!strcmp(argv[0], "pwd")) {
        if (argc == 1) {
            pwd_cmd();
        } else {
            printf("Argumente incorecte.\n");
        }
    }

    else if (!strcmp(argv[0], "cat")) {
        if (argc == 2) {
            cat_cmd(argv[1]);
        } else {
            printf("Argumente incorecte.\n");
        }
    }
    // exit and save
    else if (!strcmp(argv[0], "exit")) {
        exit_cmd();
    }
    // unknown command
    else {
        printf("Comanda necunoscuta.\n");
    }
}

/*****************************************************************/
// display the path to the current directory
void pwd_cmd() {
    unsigned char *path = malloc(1);
    find_path_of_inode(crtInode, &path);
    printf("-->%s\n", path);
}

/*****************************************************************/
// exit and save filesystem
void exit_cmd() {
    // save to buffer
    memmove(disk_buffer[0], &sb, sizeof(struct superblock));
    memmove(*(disk_buffer + sb.inode_bitmap_start), bm.inode_map, INODE_MAP_LEN);
    memmove(*(disk_buffer + sb.block_bitmap_start), bm.block_map, BLOCK_MAP_LEN);
    memmove(*(disk_buffer + sb.inode_table_start), inodes, sizeof(inodes));

    // save to "disk"
    FILE *f = fopen("filesystem.bin", "wb");
    if (f == NULL) {
        printf("Eroare la salvarea datelor.\n");
    }
    for (int i = 0; i < NR_BLOCKS; i++) {
        fwrite(disk_buffer[i], BLOCK_SIZE, 1, f);
    }

    fclose(f);

    exit(0);
}


/*****************************************************************/
// display the content of a file
void cat_cmd (unsigned char* path) {
    int file_inode = find_inode_of_path(path, crtInode, NULL, NULL);
    if (file_inode < 0) {
        printf("Nu exista fisierul.\n");
        return;
    }

    if (inodes[file_inode].file_type == 1) {
        printf("Este director.\n");
        return;
    }

    if (inodes[file_inode].file_size != 0) {
        unsigned char file[inodes[file_inode].file_size];
        extract_data(file, file_inode);
        printf("%s\n", file);
    }
}

/*****************************************************************/
// remove directory if it s empty
void rm_dir_cmd(unsigned char *path) {
    // just to not be NULL
    unsigned char* dir_name = malloc(1);
    int inode = find_inode_of_path(path, crtInode, NULL, &dir_name);
    // verify if inode is valid
    if (inode < 0 || inode > MAX_INODES || inode == ROOT_INODE_INDEX || inode == crtInode) {
        printf("Calea este incorecta.\n");
        return;
    }

    // verify if it s directory or file
    if (inodes[inode].file_type == 0) {
        printf("Nu este un director.\n");
        return;
    }

    // find parent directory
    int parent_inode = inodes[inode].parent_inode_index;
    directory parent;
    if (!extract_data(&parent, parent_inode)) {
        printf("Eroare la extragerea datelor.\n");
        return;
    }

    // extract directory data
    directory dir;
    if (!extract_data(&dir, inode)) {
        printf("Eroare la extragerea datelor.\n");
        return;
    }

    // verify if directory is empty 
    if (dir.count > 2) {
        printf("Directorul nu este gol.\n");
        return;
    }

    // reset blocks
    dir.count = 0;
    update_memory(&dir, sizeof(directory), inode);

    // reset inode
    set_bit_to_value(bm.inode_map, inode, sizeof(bm.inode_map), 0);

    // modify parent directory
    for (int i = 0; i < parent.count; i++) {
        if (parent.entries[i].inode_index == inode) {
            for (int j = i; j < parent.count - 1; j++) {
                memmove(&parent.entries[j], &parent.entries[j + 1], sizeof(directory_entry));
            }
            break;
        }
    }
    parent.count--;
    update_memory(&parent, sizeof(directory), parent_inode);

    printf("Directorul %s a fost sters.\n", dir_name);
}


/*****************************************************************/
// remove the file from specified path
void rm_file_cmd (unsigned char *path) {
    // just to not be NULL
    unsigned char *filename = malloc(1);
    int inode = find_inode_of_path(path, crtInode, NULL, &filename);

    // verify if inode is valid
    if (inode == -1 || inodes[inode].file_type == 1) {
        printf("Calea este incorecta.\n");
        return;
    }

    // extract data about parent directory
    int parent_inode = inodes[inode].parent_inode_index;
    directory dir;
    if(!extract_data(&dir, parent_inode)) {
        printf("Eroare la extragerea datelor.\n");
        return;
    }

    // update directory
    for (int i = 0; i < dir.count; i++) {
        if (dir.entries[i].inode_index == inode) {
            for (int j = i; j < dir.count - 1; j++) {
                memmove(&dir.entries[j], &dir.entries[j + 1], sizeof(directory_entry));
            }
            break;
        }
    }
    dir.count--;

    // verify the update of memory
    if (!update_memory(&dir, sizeof(directory), parent_inode)) {
        printf("Eroare.\n");
        return;
    }

    // resize the file
    int old_size = inodes[inode].file_size;
    inodes[inode].file_size = 0;

    // delete file from memory and verify
    if(!update_memory(bm.block_map, sizeof(bm.block_map), inode)) {
        printf("Nu s a putut sterge fisierul.\n");
        inodes[inode].file_size = old_size;
        return;
    }

    // reset inode
    set_bit_to_value(bm.inode_map, inode, sizeof(bm.inode_map), 0);

    // success message
    printf("Fisierul %s a fost sters.\n", filename);
}


/*****************************************************************/
// create a file with specified name
void make_file_cmd(unsigned char *path) {
    int new_inode = find_free_inode();
    if (new_inode == -1) {
        printf("Nu mai exista spatiu.\n");
        return;
    }

    int directory_inode = 0;
    // just to not be NULL
    unsigned char *filename = malloc(1);

    // if returns -2, we know that just last directory from path doesn t exist
    if (find_inode_of_path(path, crtInode, &directory_inode, &filename) != -2) {
        printf("Calea este invalida.\n");
        return;
    }

    // set inode
    inodes[new_inode].parent_inode_index = directory_inode;
    inodes[new_inode].file_type = 0;
    inodes[new_inode].crtBLocks = 0;
    set_bit_to_value(bm.inode_map, new_inode, sizeof(bm.inode_map), 1);


    directory dir;
    if (!extract_data(&dir, directory_inode)) {
        printf("Eroare la extragerea datelor!\n");
        set_bit_to_value(bm.inode_map, new_inode, sizeof(bm.inode_map), 0);
        return;
    }

    dir.entries[dir.count].inode_index = new_inode;
    strcpy(dir.entries[dir.count].filename, filename);
    dir.count++;


    if (!update_memory(&dir, sizeof(dir), directory_inode)) {
        set_bit_to_value(bm.inode_map, new_inode, sizeof(bm.inode_map), 0);
        return;
    }

    printf("Fisierul %s a fost creat cu succes.\n", filename);
}

/*****************************************************************/
// Change current directory to the specified directory name
void change_dir_cmd(unsigned char *path) {
    // initialize current directory
    directory dir;
    if(!extract_data(&dir, crtInode)) {
        printf("Eroare la extragerea datelor.\n");
        return;
    }

    int new_inode = find_inode_of_path(path, crtInode, NULL, NULL);
    if (new_inode == -1) {
        printf("Nu a fost gasit directorul.\n");
        return;
    }

    if (inodes[new_inode].file_type == 0) {
        printf("Nu a fost gasit directorul.\n");
        return;
    }

    // change current inode to the new directory
    crtInode = new_inode;
}


/*****************************************************************/
// this function create a directory with a specified name
void create_dir_cmd(unsigned char *path) {
    // to not be NULL
    unsigned char *dir_name = malloc(1);

    int parent_inode = 0;
    if (find_inode_of_path(path, crtInode, &parent_inode, &dir_name) != -2) {
        printf("Eroare.\n");
        return;
    }

    // extract data and make necessary verifications
    directory parent_dir;
    if (!extract_data(&parent_dir, parent_inode)) {
        printf("Eroare la extragerea datelor!\n");
        return;
    };

    if (parent_dir.count >= MAX_FILES_IN_DIRECTORY) {
        printf("Acest director este plin!\n");
        return;
    }

    if (strlen(dir_name) > MAX_FILE_NAME) {
        printf("Numele directorului este prea lung.\n");
        return;
    }

    for (int i = 0; i < parent_dir.count; i++) {
        if (!strcmp(parent_dir.entries[i].filename, dir_name)) {
            printf("Nu pot exista doua directoare cu acelasi nume.\n");
            return;
        }
    }

    // find inode for director
    int new_inode = find_free_inode();
    if (new_inode == -1) return;

    // set new inode
    set_bit_to_value(bm.inode_map, new_inode, sizeof(bm.inode_map), 1);
    inodes[new_inode].file_type = 1;
    inodes[new_inode].parent_inode_index = parent_inode;
    inodes[new_inode].crtBLocks = 0;

    // create new directory
    directory new_dir = {0};
    new_dir.count = 2;

    // add default "." entry
    memcpy(new_dir.entries[0].filename, ".", 1);
    new_dir.entries[0].inode_index = new_inode;

    // add default ".." entry
    memcpy(new_dir.entries[1].filename, "..", 2);
    new_dir.entries[1].inode_index = parent_inode;

    if (!update_memory(&new_dir, sizeof(new_dir), new_inode)) {
        set_bit_to_value(bm.inode_map, new_inode, sizeof(bm.inode_map), 0);
        return 0;
    }

    // update parent directory and new directory entry
    parent_dir.entries[parent_dir.count].inode_index = new_inode;
    strcpy(parent_dir.entries[parent_dir.count++].filename, dir_name);

    update_memory(&parent_dir, sizeof(parent_dir), parent_inode);

    printf("Directorul %s a fost creat cu succes.\n", dir_name);
}


/*****************************************************************/
// this function shows all entries of a directory
void list_cmd(int args, char *argv[]) {
    // will store necessary inode
    int neededInode;
    if (args == 1) {
        neededInode = crtInode;
    } else if (args == 2) {
        // there s a path
        neededInode = find_inode_of_path(argv[1], crtInode, NULL, NULL);
        if (neededInode < 0) {
            printf("Calea %s nu este corecta.\n", argv[1]);
            return;
        }
    } else {
        printf("Argumente incorecte.\n");
        return;
    }

    // will print all entries from directory
    int file_type = inodes[neededInode].file_type;
    if (file_type == 1) {
        directory dir;
        extract_data(&dir, neededInode);
        for (int i = 0; i < dir.count; i++) {
            printf("%s\n", dir.entries[i].filename);
        }
    }
}



/*****************************************************************/
// initiates filesystem: (default initialization & create root) / read disk
int filesystem_init() {
    if (superblock_init()) {
        memmove(bm.inode_map, disk_buffer[sb.inode_bitmap_start], INODE_MAP_LEN);
        memmove(bm.block_map, disk_buffer[sb.block_bitmap_start], BLOCK_MAP_LEN);
        memmove(inodes, *(disk_buffer + sb.inode_table_start), sizeof(inodes));
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
        directory root = {0};
        root.count = 2;

        char s1[] = ".";
        char s2[] = "..";
        // add default "." entry
        memcpy(root.entries[0].filename, s1, strlen(s1));
        root.entries[0].inode_index = ROOT_INODE_INDEX;

        // add default ".." entry
        memcpy(root.entries[1].filename, s2, strlen(s2));
        root.entries[1].inode_index = ROOT_INODE_INDEX;

        inodes[ROOT_INODE_INDEX].file_type = 1;
        inodes[ROOT_INODE_INDEX].parent_inode_index = ROOT_INODE_INDEX;
        set_bit_to_value(bm.inode_map, ROOT_INODE_INDEX, sizeof(bm.inode_map), 1);
        //printf("bit: %d", find_bit_value(bm.inode_map, ROOT_INODE_INDEX, sizeof(bm.inode_map)));

        update_memory(&root, sizeof(root), ROOT_INODE_INDEX);
    }
    return 1;
}


/*****************************************************************/
// this function copies data from "disk" to "buffer" and initiates suberblock
// return 1 for success
int superblock_init() {
    FILE *f = fopen(FILESYSTEM_NAME, "rb");
    int is_disk = 1;
    if (f != NULL && !file_is_empty(FILESYSTEM_NAME)) {      // if disk doesn t contain data, default initialization 
        for (int i = 0; i < NR_BLOCKS; i++) {
            fread(disk_buffer[i], BLOCK_SIZE, 1, f);
        }
        memmove(&sb, disk_buffer[0], sizeof(sb));       // read all superblock from first block of memory
        fclose(f);
    } else {
        is_disk = 0;
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

    return is_disk;
}



/*****************************************************************/
// find the full path from the current inode to the root
int find_path_of_inode(int crt_inode, unsigned char **str) {
    if (str == NULL) return 0; // for safety

    int length = 256;       // reasonable length 
    unsigned char *path = malloc(length);
    if (path == NULL) return 0;
    path[0] = '\0';

    int parent_inode;

    // move accross inodes until meeting root
    while (crt_inode != ROOT_INODE_INDEX) {
        parent_inode = inodes[crt_inode].parent_inode_index;
        directory dir;
        extract_data(&dir, parent_inode);

        // find the directory entry corresponding to the current inode
        for (int i = 0; i < dir.count; i++) {
            if (crt_inode == dir.entries[i].inode_index) {
                int name_len = strlen(dir.entries[i].filename);
                int path_len = strlen(path);

                // ensure there is enough space in the path buffer
                while (length < strlen(path) +  strlen(dir.entries[i].filename) + 2) {
                    length *= 2;
                    char *temp = realloc(path, length);
                    if (temp == NULL) {
                        free(path);
                        return 0;
                    }
                    path = temp;
                }

                // add directory name to path
                memmove(path + name_len + 1, path, path_len + 1);
                path[name_len] = '/';
                memmove(path, dir.entries[i].filename, name_len);
                break;
            }
        }

        crt_inode = parent_inode;
    }

    // ensure there is enough space for '/'
    while (length < strlen(path) + 1) {
        length++;
        char *temp = realloc(path, length);
        if (temp == NULL) {
            return 0;
        }
        path = temp;
    }

    // add '/' to path
    memmove(path + 1, path, strlen(path) + 1);
    path[0] = '/';

    free(*str);
    *str = strdup(path);
    free(path);
    return 1;
}


/*****************************************************************/
void print_path() {
    printf("%s> ", path);
}


/*****************************************************************/
// add any characters to a file
int add_word_to_file(unsigned char *content, int file_inode, int delete) {
    //verify inode
    if (find_bit_value(bm.inode_map, file_inode, sizeof(bm.inode_map)) == 0) {
        printf("Eroare.\n");
        return 0;
    } else if (inodes[file_inode].file_type == 1) {
        printf("Nu este fisier.\n");
        return 0;
    }

    if (strlen(content) + inodes[file_inode].file_size > MAX_CONTENT_IN_FILE) {
        printf("Fisierul este plin.\n");
        return 0;
    }

    // if user wants to delete the old content of file
    if (delete == 1) {
        update_memory(NULL, 0, file_inode);
    }

    int file_size = inodes[file_inode].file_size;
    int content_size = strlen(content);

    // for adding '\0'
    if (file_size == 0) file_size++;


    unsigned char *file = calloc(content_size + file_size, 1);
    if (!file) {
        printf("Eroare la alocarea memorie!\n");
        return 0;
    }
    
    file[content_size + file_size] = '\0';


    // extract old file content
    extract_data(file, file_inode);

    // add new content
    strncpy(file + file_size - 1, content, content_size);

    update_memory(file, content_size + file_size, file_inode);

    return 1;
} 


/*****************************************************************/
// parse arguments from stdin
void parse(char *in, int *argc, char *argv[]) {
    char space[] = " ";
    int count = 0;

    while (*in != '\0') {
        in += strspn(in, space);    // jump over spaces
        if (*in == '\0') break;

        argv[count++] = in;         // point to word
        in = strpbrk(in, space);
        if (in == NULL) break;      

        *in = '\0';
        in++;
    }

    *argc = count;
}

/*****************************************************************/
// verify if a file (disk) contains data from a precedent writing
// return 1 if file is empty
int file_is_empty(char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);

    fclose(f);
    return (size == 0);
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
// extract data from the blocks specified by inode and put them in the array
int extract_data(void *array, int inode_index) {
    // see if inode is valid
    if (find_bit_value(bm.inode_map, inode_index, sizeof(bm.inode_map)) == -1) {
        return 0;
    }

    // cast array
    unsigned char* arr = (unsigned char*)array;

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
    if (pos < 0 || pos >= size * BYTE_LEN) return -1;

    int index = pos / BYTE_LEN;
    int offset = pos % BYTE_LEN;
    if ((arr[index] & (1 << offset)) == 0) {
        return 0;
    }
    return 1;
}


/*****************************************************************/

/* if size of a file/directory is modified, update disk, bitmap of blocks and inode fields that manage memory.
Can be used to delete blocks, add  blocks, but another changes as delete inode, add inode,
update bm of inodes etc, should be changed manually */
int update_memory(void *a, int size, int inode_index) {
    unsigned char *arr = (unsigned char *)a;
    // calculate all blocks that we need
    int requiredBlocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (requiredBlocks > MAX_DIRECT_BLOCKS) {
        printf("Memoria alocata individual a atins maximul!\n");
        return 0;
    }

    if (!find_bit_value(bm.inode_map, inode_index, sizeof(bm.inode_map))) {
        printf("Noul inode e folosit.\n");
        return 0;
    }
    int usedBlocks = inodes[inode_index].crtBLocks;

    if (sb.free_blocks < requiredBlocks - usedBlocks) {
        printf("Nu mai exista memorie libera pe disc!\n");
        return 0;
    }

    // update data to the old blocks
    for (int i = 0; i < requiredBlocks && i < usedBlocks; i++) {
        memmove(disk_buffer[inodes[inode_index].direct_blocks[i]], arr + BLOCK_SIZE * i, BLOCK_SIZE);
    }

    // if necessary, allocate new blocks
    for (int i = usedBlocks; i < requiredBlocks; i++) {
        // looking for free block
        int free_block = find_free_block();

        // write data to the newly allocated block
        memmove(disk_buffer[free_block], arr + BLOCK_SIZE * i, BLOCK_SIZE);
        set_bit_to_value(bm.block_map, free_block, sizeof(bm.block_map), 1);
        sb.free_blocks--;
        inodes[inode_index].direct_blocks[i] = free_block;
    }

    // if necessary, delete blocks
    for (int i = requiredBlocks; i < usedBlocks; i++) {
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
int find_free_block() {
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
// find index of free inode
// return index or -1 for error
int find_free_inode() {
    if (sb.free_nodes == 0) return -1;

    for (int i = 0; i < MAX_INODES; i++) {
        if (find_bit_value(bm.inode_map, i, sizeof(bm.inode_map)) == 0) {
            return i;
        }
    }

    return -1;
}


/*****************************************************************/
//this function returns inode of specified path
//returns -2 if path is correct, but the last element isn t 
//returns -1 if path is wrong
int find_inode_of_path(unsigned char *path, int crt_inode, int *parent_inode, unsigned char **last_file_name) {
    if (path == NULL) return -1;

    unsigned char *copy_path = strdup(path);
    if (copy_path == NULL) return -1;

    // verify if it s absolute path
    if (copy_path[0] == '/') { 
        crt_inode = ROOT_INODE_INDEX;
    }

    // token will contain file/dir name
    char *token = strtok(copy_path, "/");

    if (parent_inode != NULL) {
        *parent_inode = crt_inode;
    }

    int is_file = 0;
    int exist = 1;

    while (token != NULL) {
        // copy last name from path
        if (last_file_name != NULL) {
            free(*last_file_name);
            *last_file_name = strdup(token);
        }

        if (strlen(token) > MAX_FILE_NAME) {
            free(copy_path);
            return -1;
        }

        // if path continues after a file was discovered, is incorrect
        if (is_file == 1) {
            free(copy_path);
            return -1;
        }

        directory dir;
        // if can t extract data ,directory doesn t exist
        if (!extract_data(&dir, crt_inode)) {
            free(copy_path);
            return -1;
        }

        // start_inode will be changed in the next for
        if (parent_inode != NULL) {
            *parent_inode = crt_inode;
        }

        // find file/dir
        exist = 0;
        for (int i = 0; i < dir.count; i++) {
            if (!strcmp(dir.entries[i].filename, token)) {
                exist = 1;
                crt_inode = dir.entries[i].inode_index;

                if (inodes[dir.entries[i].inode_index].file_type == 0) {
                    is_file = 1;
                }
                break;
            }
        }
        if (exist == 0) {
            // if -2 is returned, we know that last file/directory from path does n exist
            crt_inode = -2;
        }
        token = strtok(NULL, "/");
    }

    free(copy_path);
    // will return necessary inode
    return crt_inode;
}
