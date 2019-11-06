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

    if(argc != 5 && argc != 4) {
        fprintf(stderr, "Usage: %s image <path> <path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    int source_parent_dir, destination_parent_dir;
    struct ext2_dir_entry *entry;
    if (strcmp(argv[2], "-s\0") == 0) {
	char *source_file = dirname(argv[3]);
        source_parent_dir = find_dir(disk, argv[3]);
        if (is_dir_entry(disk, source_parent_dir, source_file) == 0){
        	return ENOENT; // the source name does not exist
        } else if (is_dir_entry(disk, source_parent_dir, source_file) > 1) {
		return EISDIR;
	}
	destination_parent_dir = find_dir(disk, argv[4]);
	char *destination_file = dirname(argv[4]);
	if (is_dir_entry(disk, destination_parent_dir, destination_file) == 1){
                return ENOENT; // the destination name does exist
        } else if (is_dir_entry(disk, destination_parent_dir, destination_file) > 1) {
		destination_file = dirname(argv[3]);
        }
        //allocate a free inode and set the corrosponding bit to 1 in the inode bitmap
        struct ext2_inode *inodes = (struct ext2_inode *) (disk + 1024*5);
        int inode_number = free_inode(disk);
        struct ext2_inode *file_inode = &inodes[inode_number - 1];
        file_inode->i_mode = EXT2_S_IFLNK;
        file_inode->i_links_count = 1;
	file_inode->i_size += strlen(argv[3]);
        int block_num = free_block(disk);
	file_inode->i_blocks = 2;
        unsigned char * block = (unsigned char *) (disk+ 1024 * block_num);
        memcpy(block, argv[3], strlen(argv[3]));
        file_inode->i_block[0] = block_num;
        add_dir_entry(disk, destination_parent_dir, inode_number , destination_file, EXT2_S_IFLNK);
        return 0;
    }
     int i = 0;
     char *source_file = dirname(argv[2]);
     source_parent_dir = find_dir(disk, argv[2]);
     char *destination_file = dirname(argv[3]);
     if (is_dir_entry(disk, source_parent_dir, source_file) == 0){
                return ENOENT; // the source name does not exist
     } else if (is_dir_entry(disk, source_parent_dir, source_file) > 1) {
                return EISDIR;
     }
     destination_parent_dir = find_dir(disk, argv[3]);
     if (is_dir_entry(disk, destination_parent_dir, destination_file) == 1){
                return ENOENT; // the destination name does exist
     } else if (is_dir_entry(disk, destination_parent_dir, destination_file) > 0) {
                destination_file = dirname(argv[2]);
     } else if (is_dir_entry(disk, destination_parent_dir, destination_file) == -1) {
     	        return ENOENT;
     }
 
    // find the struct inode for the source parent directory
    struct ext2_inode *inodes = (struct ext2_inode *) (disk + 1024*5);
    struct ext2_inode *source_parent_inode = &inodes[source_parent_dir - 1];
    int s; 
    // find the file's entry
    while(source_parent_inode->i_block[i] != 0){
          //intialize the first entry in the new block
          entry = (struct ext2_dir_entry *) (disk + 1024*(source_parent_inode->i_block[i]));
          s = 0;
          //set the all chars of the name string to 0 to avoid merging names
	  int k = strlen(source_file);
          if (strlen(source_file) == entry->name_len) { k = entry->name_len;}
          while (strncmp(source_file, entry->name, k) != 0 && s != 1024) {  // s is the size of entries visited
                entry = (struct ext2_dir_entry *) (disk + 1024*(source_parent_inode->i_block[i]) + s);
                s = s + entry->rec_len;
          }
          if(strncmp(source_file, entry->name, k) == 0){break;}
          i++; //move to the next block
    }

    // find the file's inode
    struct ext2_inode *file_inode = &inodes[entry->inode - 1];
    if ((file_inode->i_mode & EXT2_S_IFREG) == EXT2_S_IFREG || (file_inode->i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK){
        file_inode->i_links_count++;
        // find the struct inode of the destination parent directory
        //struct ext2_inode *source_parent_inode = &inodes[destination_parent_dir];
        //allocate a new entry for the file   
	if ((file_inode->i_mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
		add_dir_entry(disk, destination_parent_dir, entry->inode, destination_file , EXT2_S_IFREG);

	} else {
		add_dir_entry(disk, destination_parent_dir, entry->inode, destination_file, EXT2_S_IFLNK);
	}
    }
    return 0;
}
