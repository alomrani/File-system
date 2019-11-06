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
/**
	returns the last entry in the path                    
**/
char *dirname(char *path) {
        char* dir = malloc(sizeof(char) * 255);
	char temp[4096];
	strncpy(temp, path, strlen(path)+1);
        char *currentdir = strtok(temp, "/");
        while (currentdir != NULL) {
                strcpy(dir, currentdir);
                currentdir = strtok(NULL, "/");
                if (currentdir == NULL) {
                        return dir;
                }
        }
	return dir;
}
/**
        finds a free inode and allocates it in the bitmap
**/
int free_inode(unsigned char *disk) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 1024*2);
    unsigned char *inode_bits = (unsigned char *) (disk + (4)*1024);
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
    for (int byte = 0; byte < (sb->s_inodes_count / 8); byte++) {
        for (int bit = 0; bit < 8; bit++) {
                int in_use = inode_bits[byte] & (1 << bit);
                if (in_use == 0) {
                        inode_bits[byte] |= (1 << bit);
			gd->bg_free_inodes_count--;
			sb->s_free_inodes_count--;
			struct ext2_inode *inode = (struct ext2_inode *) (inodes + ((8*byte) + bit));
    			memset(inode, 0, sizeof(struct ext2_inode));
                        return (8*byte) + bit + 1;
                }
        }
    }
    exit(ENOSPC);
}
/**
        checks whether given inode is allocated, if not it will set it in the bitmap.
**/
int is_inode_allocated(unsigned char *disk, int inode) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 1024*2);
    unsigned char *inode_bits = (unsigned char *) (disk + (4)*1024);
    int index = (int ) (floor((inode-1)/8));
    if ((inode_bits[index] & (1 << ((inode-1) % 8))) == 0) {
	inode_bits[index] |= (1 << ((inode-1) % 8));
	gd->bg_free_inodes_count--;
        sb->s_free_inodes_count--;
	return 1; 
    } else {
	return 0;	

    }

}
/**
        finds the last empty block that a directory has.
**/
int free_dir_block(unsigned char *disk, int inode) {
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
    int i = 0;
    while (inodes[inode-1].i_block[i] != 0) {
        if (inodes[inode-1].i_block[i+1] == 0) {
                return i;
        }
        i++;
    }
    return 0;

}
/**
        returns a free block and allocates it in the bitmap.
**/
int free_block(unsigned char *disk) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 1024*2);
    unsigned char *block_bits = (unsigned char *) (disk + (3)*1024);
    int in_use;
    for (int byte = 0; byte < (sb->s_blocks_count / 8); byte++) {
        for (int bit = 0; bit < 8; bit++) {
                in_use = block_bits[byte] & (1 << bit);
                if (in_use == 0) {
                        block_bits[byte] |= (1 << bit);
			gd->bg_free_blocks_count--;
			sb->s_free_blocks_count--;
			unsigned char *block = disk + (((8* byte) + bit + 1) * EXT2_BLOCK_SIZE);
    			memset(block, 0, EXT2_BLOCK_SIZE);
                        return (8 *byte) + bit + 1;
                }
        }
    }
    exit(ENOSPC);

}
/**
        checks whether given inode's blocks are all allocated and sets them in bitmap if not.
**/
int is_block_allocated(unsigned char *disk, int inode) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 1024*2);
    unsigned char *block_bits = (unsigned char *) (disk + (3)*1024);
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
    int i = 0;
    int blocks_fixed = 0;
    int index;
    while (inodes[inode - 1].i_block[i] != 0) {
	index = (int ) (floor((inodes[inode - 1].i_block[i]-1)/8));
	if ((block_bits[index] & (1 << ((inodes[inode - 1].i_block[i]-1) % 8))) == 0) {
        	block_bits[index] |= (1 << ((inodes[inode - 1].i_block[i]-1) % 8));
		blocks_fixed++;
		gd->bg_free_blocks_count--;
                sb->s_free_blocks_count--;
	}
	i++;

    }
    return blocks_fixed;



}
/**
        checks whether given entry name exists, return the inode number if its a dir, 1 if its a file, and 0 if it does not exist or if its a link.
**/
int is_dir_entry(unsigned char* disk, int parent_dir, char*name) {
        int j = 0;
        int i = 0;
        struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
        while (j < inodes[parent_dir - 1].i_blocks/(2) && inodes[parent_dir - 1].i_block[j] != 0) {
                while (i < 1024) {
                        struct ext2_dir_entry *dir = (struct ext2_dir_entry *) (disk + ((1024) * (inodes[parent_dir - 1].i_block[j])) + i);
                        char * namedir = (char * ) (disk + sizeof(struct ext2_dir_entry *) + ((1024) * (inodes[parent_dir - 1].i_block[j])) + i);
//			char temp[dir->name_len-1];
//			strncpy(temp, namedir, dir->name_len);
//			char temp2[strlen(name)];
//			strncpy(temp2, name, strlen(name));
			int k = strlen(name);
			if (strlen(name) == dir->name_len) { k = dir->name_len;}
                        if ((inodes[dir->inode - 1].i_mode & EXT2_S_IFREG) == EXT2_S_IFREG && strlen(name) == dir->name_len && strncmp(name, namedir, k) == 0) {
                                return 1;
                        } else if ((inodes[dir->inode-1].i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR && strlen(name) == dir->name_len) {
                                if (strncmp(name, namedir, k) == 0) {
                                        return dir->inode;
                                }
                        } else if ((inodes[dir->inode-1].i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK && strlen(name) == dir->name_len) {
				if (strncmp(name, namedir, k) == 0) {
                                        return -1;
                                }
			}
                        i = i + dir->rec_len;
                }
                j++;
                i = 0;
        }
        return 0;




}
/**
        Makes sure everything in the path is valid, returns the parent dir inode number of last entry in path.
**/
int find_dir(unsigned char *disk, char *path) {
        char *token;
	char *next;
	char temp[4096];
	strncpy(temp, path, strlen(path));
        token = strtok(temp, "/");
	while (token != NULL && strcmp("", token) == 0) {
		token = strtok(NULL, "/");
	}
	next = strtok(NULL, "/");
	while (next != NULL && strcmp("", next) == 0) {
                next = strtok(NULL, "/");
        }
        int current = 1;
	int num;
        while(token != NULL) {
			if (next == NULL) {
				return current+1;
			}
                        if (is_dir_entry(disk, current + 1, token) == 1) {
				if (next == NULL) {
                                	return current+1;
                                } else {
                                        exit(ENOENT);
                                }
			} else if ((num = is_dir_entry(disk, current+1, token)) > 0) {
				current = num - 1;
				if (next == NULL) {
					return current +1;
				}	
			} else {
				exit(ENOENT);
			}
			token = next;
			next = strtok(NULL, "/");
			while (next != NULL && strcmp("", next) == 0) {
                		next = strtok(NULL, "/");
			}	
        }
        return current + 1;
}
/**
        adds a dir entry to given parent_dir.
**/
int add_dir_entry(unsigned char* disk,int parent_dir ,int inode, char*name, int file_type) {
    int i = 0;
    int free_dir = free_dir_block(disk, parent_dir);
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
    while (i < 1024) {
        struct ext2_dir_entry *dir = (struct ext2_dir_entry *) (disk + (1024) * inodes[parent_dir-1].i_block[free_dir] + i);
        if (i + dir->rec_len == 1024) {
                if (dir->rec_len < (floor((strlen(name)+8)/4)+1)*4) {
                        if (free_dir + 1 < 12) {
                                inodes[parent_dir-1].i_block[free_dir + 1] = free_block(disk);
				inodes[parent_dir-1].i_blocks += 2;
				inodes[parent_dir-1].i_size += EXT2_BLOCK_SIZE;
                                struct ext2_dir_entry *newdir = (struct ext2_dir_entry *) (disk + (1024) * inodes[parent_dir-1].i_block[free_dir + 1]);
                                newdir->inode = inode;
                                newdir->name_len = strlen(name);
                                newdir->rec_len = EXT2_BLOCK_SIZE;
                                strncpy(newdir->name, name, strlen(name));
                                inodes[inode - 1].i_mode = file_type;
                                if (file_type == EXT2_S_IFREG) {
                                	newdir->file_type = EXT2_FT_REG_FILE;
                        	} else if (file_type == EXT2_S_IFDIR) {
                                	newdir->file_type = EXT2_FT_DIR;
                        	} else if (file_type == EXT2_S_IFLNK) {
					newdir->file_type = EXT2_FT_SYMLINK;
				}
				return free_dir + 1;
                        } else {
                                exit(ENOSPC);
                        }

			//INCREASE I_SIZE OF DIRECTORY

                } else {
                        struct ext2_dir_entry *newdir = (struct ext2_dir_entry *) (disk + (1024) * inodes[parent_dir-1].i_block[free_dir] + i + (int )(floor((dir->name_len+8)/4)+1)*4)
;
                        newdir->rec_len = dir->rec_len - (floor((dir->name_len+8)/4)+1)*4;
                        dir->rec_len = (floor((dir->name_len+8)/4)+1)*4;
                        newdir->inode = inode;
                        newdir->name_len = strlen(name);
                        strncpy(newdir->name, name, strlen(name));
                        inodes[inode-1].i_mode = file_type;
			if (file_type == EXT2_S_IFREG) {
                        	newdir->file_type = EXT2_FT_REG_FILE;
			} else if (file_type == EXT2_S_IFDIR) {
				newdir->file_type = EXT2_FT_DIR;
				inodes[parent_dir-1].i_links_count++;
			} else if (file_type == EXT2_S_IFLNK) {
				newdir->file_type = EXT2_FT_SYMLINK;

			}
                        break;
                }
        }
        i = i + dir->rec_len;
    }
    return 0;	
}

/**
       returns 0 if the file represented by inode is restorable and 1 otherwise. Exists if the target is a directory.
**/
int check_blocks(unsigned char *disk, int inode) {
    unsigned char *block_bits = (unsigned char *) (disk + (3)*1024);
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
    int i = 0;
    int zero_bits = 0;
    int index;
    if((inodes[inode -1].i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR){
       exit(EISDIR);
    }
    while (inodes[inode - 1].i_block[i] != 0) {
        index = (int ) (floor((inodes[inode - 1].i_block[i]-1)/8));
        if ((block_bits[index] & (1 << ((inodes[inode - 1].i_block[i]-1) % 8))) == 0) {
                zero_bits++;
        }
        i++;
    }
    if(zero_bits == inodes[inode -1].i_blocks){
       is_block_allocated(disk, inode);
       return 0;
    }
    return 1;
}

/**
        check if the file represented by file_name it is restorable and is the file we want to restore.
**/
int restore_entry(unsigned char *disk, int name_len, int rec_len, int inode, char* file_name, int i){
       int padding = (floor((name_len+8)/4)+1)*4;
       // if the next entry has been deleted
       if(rec_len != padding){
          struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
          struct ext2_inode parent_inode = inodes[inode - 1];
          struct ext2_dir_entry * new_entry = (struct ext2_dir_entry *) (disk + 1024*((parent_inode.i_block)[i]) + padding);
	  int k = strlen(file_name);
          if (strlen(file_name) == new_entry->name_len) { k = new_entry->name_len;}
          //if the next entry is the one we want to restore
          if(strncmp(new_entry->name, file_name,k) == 0){
                  //if the inode is already allocated
                  if(is_inode_allocated(disk, new_entry->inode) == 0 || (check_blocks(disk, new_entry->inode))){
                      exit(ENOENT); //file cannot be restored
                  }
                  //entry->rec_len = entry->name_len + padding + 8; //restore file
                  return 1; 
          }
       }
       return 0;
}
