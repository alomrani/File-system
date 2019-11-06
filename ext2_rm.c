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
#include <time.h>
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
    // check if the path given is valid
    int parent_dir;
    parent_dir = find_dir(disk, argv[2]);
    // find the struct inode for the parent directory
    struct ext2_inode *inodes = (struct ext2_inode *) (disk + 1024*5);
    struct ext2_inode *parent_inode = &inodes[parent_dir - 1];
    char *file_name = dirname(argv[2]);
    // find the ext2_dir_entry corrosponding to the file
    if (is_dir_entry(disk, parent_dir, file_name) == 0) {
	return ENOENT;

    } else if (is_dir_entry(disk, parent_dir, file_name) > 1) {
	return EISDIR;


    }
    int i = 0; int s;
    struct ext2_dir_entry * entry;
    struct ext2_dir_entry * prev_entry;
    while(parent_inode->i_block[i] != 0){
          //intialize the first entry in the new block
          entry = (struct ext2_dir_entry *) (disk + 1024*(parent_inode->i_block[i]));
          s = 0;
          //set the all chars of the name string to 0 to avoid merging names
	  s = s + entry->rec_len;
	  int k = strlen(file_name);
          if (strlen(file_name) == entry->name_len) { k = entry->name_len;}
          while(strncmp(file_name, entry->name, k) != 0 && s != 1024){  // s is the size of entries visited
                prev_entry = entry;
          	entry = (struct ext2_dir_entry *) (disk + 1024*(parent_inode->i_block[i]) + s);
		s = s + entry->rec_len;
          }
          if(strncmp(file_name, entry->name, k) == 0){break;}
          i++; //move to the next block
    }
    

    //find the inode struct for the file
    struct ext2_inode *file_inode = &inodes[entry->inode - 1];
    // if the file is a regular file
    if((file_inode->i_mode & EXT2_S_IFREG) == EXT2_S_IFREG || (file_inode->i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK){
       //update links count
       file_inode->i_links_count--;
       if(file_inode->i_links_count == 0){
          file_inode->i_dtime = time(NULL);
          //change the inode bit in the inode bitmap
          unsigned char *inode_bits = (unsigned char *)(disk + 1024 * 4);
          int byte_num = floor(((entry->inode-1)/8));
          inode_bits[byte_num] = inode_bits[byte_num] & ~(1 << ((entry->inode -1) % 8));
       
          //change the corrosponding bit in the block bitmap 
          unsigned char *block_bits = (unsigned char *)(disk + 1024 * 3);
          int j = 0;
          while (file_inode->i_block[j] != 0 && j < 12) {
               byte_num = (int ) floor(((file_inode->i_block[j] - 1)/8));
               block_bits[byte_num] = block_bits[byte_num] & ~(1 << ((file_inode->i_block[j] - 1) % 8)); 
               j++;
          }
	  if ((file_inode->i_blocks)/2 > 12) {
	      unsigned int * indirect_block;
	      indirect_block = (unsigned int *) (disk + (file_inode->i_block[12]) * 1024);
	      while (j < (file_inode->i_blocks - 1)/2) {
	          byte_num = (int ) floor(((*indirect_block - 1)/8));
		  block_bits[byte_num] = block_bits[byte_num] & ~(1 << ((*indirect_block - 1) % 8));
		  indirect_block++;
		  j++;

	      }


	  }
          //update ext2_group_desc
          struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 1024*2);
	  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
          gd->bg_free_inodes_count++;
	  sb->s_free_inodes_count++;
          gd->bg_free_blocks_count = gd->bg_free_blocks_count + j;
	  sb->s_free_blocks_count = sb->s_free_blocks_count + j;
       }
       //remove entry
       if(prev_entry->inode == entry->inode){
            entry->inode = 0;
       }else{
            prev_entry->rec_len = prev_entry->rec_len + entry->rec_len;
       }
    } 
    return 0; 
}
