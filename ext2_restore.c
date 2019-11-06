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
    char path[4096];
    strncpy(path, argv[2], strlen(argv[2]));
    if((parent_dir = find_dir(disk, path)) == ENOENT){
       return ENOENT;
    }
    char *file_name = dirname(path);
    if (is_dir_entry(disk, parent_dir, file_name) > 0) {
	return EEXIST;

    }
    // find the struct inode for the parent directory
    struct ext2_inode *inodes = (struct ext2_inode *) (disk + 1024*5);
    struct ext2_inode *parent_inode = &inodes[parent_dir - 1];
    //the name of file to be restored
    struct ext2_dir_entry * entry;
    int s; //the sth entry in the ith block
    int found = 0;
    int i = 0;
    while(found != 1 && parent_inode->i_block[i] != 0) {
       entry = (struct ext2_dir_entry*) (disk + 1024* parent_inode->i_block[i]);
       s = 0;
       while((found = restore_entry(disk, entry->name_len, entry->rec_len,parent_dir, file_name, i)) != 1 && s != 1024){
            entry = (struct ext2_dir_entry*) (disk + 1024* parent_inode->i_block[i] + s);
            s = s + entry->rec_len;
       }
    i++;  //move to the next block
    }
    if(found == 1){
       int padding = 4 - ((entry->name_len +8) % 4);
       entry->rec_len = entry->name_len + padding + 8; //restore file
    }
    return 0;
}
