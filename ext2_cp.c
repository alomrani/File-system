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

    if(argc != 4) {
        fprintf(stderr, "Usage: %s  image <path> <path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    int parent_dir;
    char path[4096];
    strncpy(path, argv[3], strlen(argv[3]));
    char *file_name1 = dirname(path);
    strncpy(path, argv[3], strlen(argv[3]));
    parent_dir = find_dir(disk, path);
    if (is_dir_entry(disk, parent_dir, file_name1) > 0) {
	strncpy(path, argv[2], strlen(argv[2]));
	file_name1 = dirname(path);
    } else if (is_dir_entry(disk, parent_dir, file_name1) == 1){
	return EEXIST;
    }
    FILE *fd1 = fopen(argv[2], "rb");
    if (fd1 == NULL) {
	return ENOENT;
    }
    int inode = free_inode(disk);
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
    unsigned char buf[EXT2_BLOCK_SIZE];
    add_dir_entry(disk, parent_dir ,inode, file_name1, EXT2_S_IFREG);
    int num_bytes;
    int i = 0;
    int current_block;
    unsigned int * indirect_block;
    unsigned char *file_block;
    // FIND FILE MODE
    inodes[inode-1].i_size = 0;
    while ((num_bytes = fread(buf, 1, EXT2_BLOCK_SIZE, fd1)) > 0) {
	if (i < 12) {
		current_block = (free_block(disk));
		inodes[inode-1].i_block[i] = current_block;
	} else if (i == 12) {
		inodes[inode-1].i_block[i] = free_block(disk);
		indirect_block = (unsigned int *) (disk + (inodes[inode-1].i_block[i]) * 1024);
		inodes[inode-1].i_blocks += 2;
		*indirect_block = free_block(disk);
	} else {
		indirect_block++;
		*indirect_block = free_block(disk);
	}
	inodes[inode-1].i_blocks += 2;
	inodes[inode-1].i_size += num_bytes;
	if (i < 12) {
		file_block = (unsigned char *) (disk + (current_block) * 1024);
	} else {
		file_block = (unsigned char *) (disk + (*indirect_block) * 1024);

	}
   	memcpy(file_block, buf, num_bytes);
        i++;
    }
    inodes[inode-1].i_dtime = 0;
    inodes[inode-1].i_links_count = 1; 
    inodes[inode-1].i_mode |= EXT2_S_IFREG;
    fclose(fd1);

} 
