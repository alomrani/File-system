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

    if(argc != 2) {
        fprintf(stderr, "Usage: %s diskimage \n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    int offset = 0;
    int num_fixes = 0;
    int free_inodes = 0;
    int free_blocks = 0;
    int in_use;
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 1024*2);
    unsigned char *block_bits = (unsigned char *) (disk + (3)*1024);
    unsigned char *inode_bits = (unsigned char *) (disk + (4)*1024);
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + 1024*5);
    for (int byte = 0; byte < (sb->s_inodes_count / 8); byte++) {
        for (int bit = 0; bit < 8; bit++) {
                int in_use = inode_bits[byte] & (1 << bit);
                if (in_use == 0) {
                        free_inodes++;
                }
        }
    }
    if (free_inodes != sb->s_free_inodes_count) { 
	offset = abs(sb->s_free_inodes_count - free_inodes);
    	sb->s_free_inodes_count = free_inodes;
	printf("Fixed: superblock's free inodes counter was off by %d\n", offset);
	num_fixes += offset;

    }

    if (free_inodes != gd->bg_free_inodes_count) {
	offset = abs(sb->s_free_inodes_count - free_inodes);
	gd->bg_free_inodes_count = free_inodes;
	printf("Fixed: block group's free inodes counter was off by %d\n", offset);
        num_fixes += offset;
	
    }	
    for (int byte = 0; byte < (sb->s_blocks_count / 8); byte++) {
        for (int bit = 0; bit < 8; bit++) {
                in_use = block_bits[byte] & (1 << bit);
                if (in_use == 0) {
                        free_blocks++;
                }
        }
    }
    if (free_blocks != sb->s_free_blocks_count) {
        offset = abs(sb->s_free_blocks_count - free_blocks);
        sb->s_free_inodes_count = free_blocks;
        printf("Fixed: superblock's free blocks counter was off by %d\n", offset);
        num_fixes += offset;

    }

    if (free_blocks != gd->bg_free_blocks_count) {
        offset = abs(gd->bg_free_blocks_count - free_blocks);
        gd->bg_free_blocks_count = free_blocks;
        printf("Fixed: block group's free blocks counter was off by %d\n", offset);
        num_fixes += offset;

    }
    int i = 0;
    int *dirs = malloc(sizeof(int) * 4000);
    dirs[0] = 1;
    int j1 = 1;
    int curr_block = 0;
    while (j1 > 0) {
        int current = dirs[j1-1];
	while (inodes[current].i_block[curr_block] != 0) {
		i = 0;
	        while (i < 1024) {
        	        struct ext2_dir_entry *dir = (struct ext2_dir_entry *) (disk + (1024) * inodes[current].i_block[curr_block] + i);
			char *namedir = (char *) (sizeof(struct ext2_dir_entry *) + disk + (1024) * inodes[current].i_block[curr_block] + i);
                	if ((inodes[dir->inode - 1].i_mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
                        	if ((dir->file_type & EXT2_FT_REG_FILE) != EXT2_FT_REG_FILE) {
					dir->file_type |= EXT2_FT_REG_FILE;
					printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir->inode);
					num_fixes++;
				}
                	} else if ((inodes[dir->inode - 1].i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
                        	if ((dir->file_type & EXT2_FT_DIR) != EXT2_FT_DIR) {
                                        dir->file_type |= EXT2_FT_DIR;
                                        printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir->inode);
                                        num_fixes++;
                                }
                        	if (dir->inode != 11 && dir->inode != 2 && dir->inode != current+1 && strcmp(namedir,"..") != 0) {
                                	dirs[j1-1] = dir->inode - 1;
                                	j1++;
                        	}
                	} else if ((inodes[dir->inode - 1].i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
                        	if ((dir->file_type & EXT2_FT_SYMLINK) != EXT2_FT_SYMLINK) {
                                        dir->file_type |= EXT2_FT_SYMLINK;
                                        printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir->inode);
                                        num_fixes++;
                                }	
                	}
                	i = i + dir->rec_len;
        	}
		curr_block++;
	}
	curr_block = 0;
        i = 0;
        j1--;
    }
    free(dirs);
    i = 0;
    dirs = malloc(sizeof(int) * 4000);
    dirs[0] = 1;
    j1 = 1;
    curr_block = 0;
    while (j1 > 0) {
        int current = dirs[j1-1];
        while (inodes[current].i_block[curr_block] != 0) {
                i = 0;
                while (i < 1024) {
                        struct ext2_dir_entry *dir = (struct ext2_dir_entry *) (disk + (1024) * inodes[current].i_block[curr_block] + i);
                        char *namedir = (char *) (sizeof(struct ext2_dir_entry *) + disk + (1024) * inodes[current].i_block[curr_block] + i);
                        int output = is_inode_allocated(disk, dir->inode);
			if (output == 1) {
				printf("Fixed: inode [%d] not marked as in-use\n", dir->inode);
				num_fixes++;
			}
			if ((inodes[dir->inode - 1].i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
                                if (dir->inode != 11 && dir->inode != 2 && dir->inode != current+1 && strcmp(namedir,"..") != 0) {
                                        dirs[j1-1] = dir->inode - 1;
                                        j1++;
                                }
			}
                        i = i + dir->rec_len;
                }
                curr_block++;
        }
        curr_block = 0;
        i = 0;
        j1--;
    }
    free(dirs);
    i = 0;
    dirs = malloc(sizeof(int) * 4000);
    dirs[0] = 1;
    j1 = 1;
    curr_block = 0;
    while (j1 > 0) {
        int current = dirs[j1-1];
        while (inodes[current].i_block[curr_block] != 0) {
                i = 0;
                while (i < 1024) {
                        struct ext2_dir_entry *dir = (struct ext2_dir_entry *) (disk + (1024) * inodes[current].i_block[curr_block] + i);
                        char *namedir = (char *) (sizeof(struct ext2_dir_entry *) + disk + (1024) * inodes[current].i_block[curr_block] + i);
                        if (inodes[dir->inode-1].i_dtime != 0) {
				inodes[dir->inode-1].i_dtime = 0;
                                printf("Fixed: valid inode marked for deletion: [%d]\n", dir->inode);
                                num_fixes++;
                        }
                        if ((inodes[dir->inode - 1].i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
                                if (dir->inode != 11 && dir->inode != 2 && dir->inode != current+1 && strcmp(namedir,"..") != 0) {
                                        dirs[j1-1] = dir->inode - 1;
                                        j1++;
                                }
                        }
                        i = i + dir->rec_len;
                }
                curr_block++;
        }
        curr_block = 0;
        i = 0;
        j1--;
    }
    free(dirs);
    i = 0;
    dirs = malloc(sizeof(int) * 4000);
    dirs[0] = 1;
    j1 = 1;
    int blocks_fixed = 0;
    curr_block = 0;
    while (j1 > 0) {
        int current = dirs[j1-1];
        while (inodes[current].i_block[curr_block] != 0) {
                i = 0;
                while (i < 1024) {
                        struct ext2_dir_entry *dir = (struct ext2_dir_entry *) (disk + (1024) * inodes[current].i_block[curr_block] + i);
                        char *namedir = (char *) (sizeof(struct ext2_dir_entry *) + disk + (1024) * inodes[current].i_block[curr_block] + i);
			blocks_fixed = is_block_allocated(disk, dir->inode);
                        if (blocks_fixed > 0) {
                                printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", blocks_fixed ,dir->inode);
                                num_fixes++;
                        }
                        if ((inodes[dir->inode - 1].i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
                                if (dir->inode != 11 && dir->inode != 2 && dir->inode != current+1 && strcmp(namedir,"..") != 0) {
                                        dirs[j1-1] = dir->inode - 1;
                                        j1++;
                                }
                        }
                        i = i + dir->rec_len;
                }
                curr_block++;
        }
        curr_block = 0;
        i = 0;
        j1--;
    }
    free(dirs);
} 
