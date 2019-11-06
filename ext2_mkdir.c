#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <math.h>
#include <errno.h>
#include "ext2_utils.h"
unsigned char *disk;


int main(int argc, char **argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: %s <path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 1024*2);
    int parent_dir;
    char path[4096];
    strncpy(path, argv[2], strlen(argv[2]));
    parent_dir = find_dir(disk, path);
    char *newdir_name1 = dirname(path);
    if (is_dir_entry(disk, parent_dir, newdir_name1) > 0) {
	return EEXIST;
    }
    int inode = free_inode(disk);
    int block = free_block(disk);
    if (inode == ENOSPC || block == ENOSPC) {
	return ENOSPC;

    }
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
    inodes[inode-1].i_block[0] = block;
    inodes[inode-1].i_blocks = 2;
    inodes[inode-1].i_size = EXT2_BLOCK_SIZE;
    inodes[inode-1].i_links_count = 2;
    struct ext2_dir_entry *newdir = (struct ext2_dir_entry *) (disk + (1024) * inodes[inode-1].i_block[0]);
    newdir->inode = inode;
    strncpy(newdir->name, ".", 1);
    newdir->name_len = 1;
    newdir->rec_len = 12;
    newdir->file_type |= EXT2_FT_DIR;
    struct ext2_dir_entry *newdir1 = (struct ext2_dir_entry *) (disk + (1024) * inodes[inode-1].i_block[0] + 12);
    newdir1->inode = parent_dir;
    strncpy(newdir1->name, "..", 2);
    newdir1->name_len = 2;
    newdir1->rec_len = 1012;
    newdir1->file_type |= EXT2_FT_DIR;
    strncpy(path, argv[2], strlen(argv[2]));
    char *newdir_name = dirname(path);
    add_dir_entry(disk, parent_dir,inode ,newdir_name, EXT2_S_IFDIR);
    free(newdir_name);  
    gd->bg_used_dirs_count++;    

}
