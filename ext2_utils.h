#include <string.h>
#include "ext2.h"










char * dirname(char* path);
int find_dir(unsigned char *disk, char * path);
int free_dir_block(unsigned char *disk, int inode);
int free_inode(unsigned char *disk);
int free_block(unsigned char *disk);
int is_inode_allocated(unsigned char *disk, int inode);
int is_block_allocated(unsigned char *disk, int inode);
int add_dir_entry(unsigned char *disk, int parent_dir,int inode, char* name, int file_type);
int is_dir_entry(unsigned char *disk, int parent_dir, char* name);
int restore_entry(unsigned char *disk, int name_len, int rec_len, int inode, char* file_name, int i);
int check_blocks(unsigned char *disk, int inode);
