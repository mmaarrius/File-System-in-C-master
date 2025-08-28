This is a personal project made for fun, through which I gained a foundational understanding of filesystem functionality. 

# FileSystem Project
This repository represents a simple simulation of a File System.

![operating_principle](poza.jpg)

## About EXT2
Ext2 or Second Extended File System was the default file system for Linux kerner until supplanted by Ext3 in 2001.

Ext2 divides the disk into multiple block groups. Each block group contains:
- Superblock - stores metadata about filesystem (size, free blocks, inode count, etc.)
- Block Bitmap - keeps track about allocated/free blocks
- Inode Bitmap - keeps track about allocated/free inodes
- Inode Table - stores metadata about files/directories
- Data Blocks - store actual file content

## Operating Principle
Every structure mentioned above is read from disk. When a file or directory is created, an entry in the directory is also created. This entry stores the allocated inode and the name of the file/folder.

• How can the system keep the content of a file?  
It simply stores the content in blocks and keeps track of which blocks contain the data.

• How many blocks can a single file use?  
A lot—enough to store its entire content. Blocks are allocated as the file size increases.  
• 12 direct blocks: store data  
• 1 indirect block: stores pointers to other data blocks  
• 1 double indirect block: stores pointers to blocks that contain pointers to other data blocks  
• 1 triple indirect block: stores pointers to blocks that contain pointers to blocks with more pointers to data blocks  
• As a result, the number of blocks can grow significantly.

• What is an inode?  
An inode keeps track of the allocated blocks for a file/folder, the inode of the directory where the file/folder is stored, permissions, and timestamps. When you run "ls", all displayed data is retrieved from the inode table.

• What about directories?  
The same principle applies as for files, but instead of storing file data, the blocks contain entries (file names and inode indices).

## Commands
| Command                     | Description                |
|:----------------------------|:---------------------------|
|`mkdir <path/directory_name>`| create new directory       |
|`touch <path/file_name>`     | create new file            |
|`echo <content> >/>> <path>` | add content to a file      |
|`rm <path_to_file>`          | remove a file              |
|`rmdir <path_to_direvtory>`  | remove an empty directory  |
|`cd <path_to_directory>`     | change current directory   |
|`ls <path_to_directory>`     | list entries of direvtory  |
|`pwd`                        | show path                  |
|`cat <path_to_file>`         | show content of file       |


## Running
just do :  
`gcc main.c -o main`  
and :  
`./main`
