#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "ext2_fs.h"
#include "read_ext2.h"

int read_inode_data(int fd, off_t offset, char *buffer, size_t len) {
    lseek(fd, BLOCK_OFFSET(offset), SEEK_SET);
    return read(fd, buffer, len);
}

int check_jpg(char *buffer) {
    int is_jpg = 0;
    if (buffer[0] == (char)0xff && buffer[1] == (char)0xd8 &&
        buffer[2] == (char)0xff &&
        (buffer[3] == (char)0xe0 || buffer[3] == (char)0xe1 ||
         buffer[3] == (char)0xe8)) {
        is_jpg = 1;
    }
    return is_jpg;
}

int check_inode_jpg(int fd, off_t start, int inode_no) {
    int is_jpg = 1;
    struct ext2_inode inode;
    // read inode data to buffer
    read_inode(fd, 0, start, inode_no, &inode);
    char buffer[block_size];
    int read = read_inode_data(fd, inode.i_block[0], buffer, block_size);
    if (read < 0 || !check_jpg(buffer)) {
        // if not jpeg
        is_jpg = 0;
    }
    return is_jpg;
}

int copy_data(int fd, char *filename, off_t start, int inode_no) {
    struct ext2_inode inode;
    read_inode(fd, 0, start, inode_no, &inode);

    if (!S_ISREG(inode.i_mode)) return -1;
    if (inode.i_blocks == 0) return 0;

    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int size = inode.i_size;

    int curr = 0;

    // direct blocks
    while (curr < EXT2_NDIR_BLOCKS && size > 0) {
        char buffer[block_size];
        int read =
            read_inode_data(fd, inode.i_block[curr], buffer,
                            size > (int)block_size ? (int)block_size : size);
        write(file, buffer, read);
        size -= read;
        curr++;
    }

    // indirect blocks
    if (size > 0) {
        int blocks[block_size / sizeof(int)];
        int *arr = blocks;

        read_inode_data(fd, inode.i_block[EXT2_IND_BLOCK], (char *)blocks,
                        block_size);
        int iterations = block_size / sizeof(int);

        while (iterations-- > 0 && size > 0) {
            char buffer[block_size];
            int read = read_inode_data(
                fd, *arr, buffer,
                size > (int)block_size ? (int)block_size : size);

            write(file, buffer, read);
            size -= read;
            arr++;
        }
    }

    // double indirect blocks
    if (size > 0) {
        int ind_blocks[block_size / sizeof(int)];
        int *ind_arr = ind_blocks;

        read_inode_data(fd, inode.i_block[EXT2_DIND_BLOCK], (char *)ind_blocks,
                        block_size);
        int out_iterations = block_size / sizeof(int);

        while (out_iterations-- > 0 && size > 0) {
            int blocks[block_size / sizeof(int)];
            int *arr = blocks;

            read_inode_data(fd, *ind_arr, (char *)blocks, block_size);
            int iterations = block_size / sizeof(int);

            while (iterations-- > 0 && size > 0) {
                char buffer[block_size];

                int read = read_inode_data(
                    fd, *arr, buffer,
                    size > (int)block_size ? (int)block_size : size);

                write(file, buffer, read);
                size -= read;
                arr++;
            }
            ind_arr++;
        }
    }

    close(file);
    return 0;
}

int main(int argc, char **argv) {
    char buffer[1024];
    char filename[1024]; 
    int file_list [1024];
    int fileCounter = 0;
    if (argc != 3) {
        printf("expected usage: ./runscan inputfile outputfile\n");
        exit(0);
    }

    int fd;

    fd = open(argv[1], O_RDONLY); /* open disk image */
    if (fd < 0) {
        printf("runscan: cannot open disk image\n");
        exit(0);
    }

    // try create dir
    if (opendir(argv[2]) != NULL) {
        printf("runscan: output directory already exists\n");
        exit(0);
    }
    // create dir
    int dir = mkdir(argv[2], S_IRWXU);
    if (dir < 0) {
        printf("runscan: could not create output directory\n");
        exit(0);
    }

    ext2_read_init(fd);

    struct ext2_super_block super;
    struct ext2_group_desc group;

    read_super_block(fd, 0, &super);
    read_group_desc(fd, 0, &group);
    off_t start_inode_table;

    int global_inode_no = 0;

    for (int ngroup = 0; ngroup < 1; ngroup++) {
        struct ext2_group_desc *group =
            (struct ext2_group_desc *)malloc(sizeof(struct ext2_group_desc));

        read_group_desc(fd, ngroup, group);
        start_inode_table = locate_inode_table(ngroup, group);

        for (unsigned int inode_no = 1; inode_no < inodes_per_group;
             inode_no++) {
            struct ext2_inode inode;

            read_inode(fd, 0, start_inode_table, inode_no, &inode);

            if (inode.i_blocks == 0) continue;
            if (!S_ISREG(inode.i_mode)) continue;

            int read =
                read_inode_data(fd, inode.i_block[0], buffer, block_size);
            if (read < 0 || !check_jpg(buffer)) {
                continue;
            }
            sprintf(filename, "%s/file-%d.jpg", argv[2],
                    inode_no + global_inode_no);

            copy_data(fd, filename, start_inode_table, inode_no);
            file_list[fileCounter] = inode_no;
            fileCounter++;
            //printf("%d\n", fileCounter);
            int fd = open(filename , O_RDWR);
            //printf("%s",filename);
            ftruncate( fd, inode.i_size);
            close(fd);
        }

        global_inode_no += inodes_per_group;

        
    }
    // part 2
    unsigned int totalblocks = itable_blocks * inodes_per_block;
    for (unsigned int i = 0; i < totalblocks; i++) { 
        struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
        read_inode(fd, 0, start_inode_table, i, inode);
        unsigned int i_blocks = inode->i_blocks/(2<<super.s_log_block_size);
        if (i_blocks == 0) {
            continue;
        }
        if (S_ISDIR(inode->i_mode)) {
            struct ext2_dir_entry *dir_entry = malloc(sizeof(struct ext2_dir_entry));
            int offsetNumber = 0 ;

            lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
            memset (buffer, 0, block_size);
            read(fd, buffer, 1024);

            //printf("here is a bug");
            while (offsetNumber < (int)block_size){ 
                //printf("here1");
                dir_entry = (struct ext2_dir_entry*) & (buffer[0 + offsetNumber]);

                struct ext2_inode *tempInNode = malloc(sizeof(struct ext2_inode));
                read_inode(fd, 0, start_inode_table, dir_entry->inode, tempInNode);
                
                int new_length = dir_entry->name_len & 0xFF; 
                char new_name [EXT2_NAME_LEN];

                strncpy(new_name, dir_entry->name, new_length);
                new_name[new_length] = '\0';

                if (S_ISREG(tempInNode->i_mode)) {
                    for (int i = 0; i < fileCounter; i++){
                        if ((int)dir_entry->inode == file_list[i]){
                            char* buffer = malloc(sizeof(char) * tempInNode->i_size);
                            //printf("here2");
                            char* inode_num = malloc(sizeof(i)); 
                            sprintf(inode_num, "%d", dir_entry->inode);
                            //./length is 2, / length is 1, /file- length is 6 and .jpg length is 4
                            int len = 2 + strlen(argv[2]) + 6 + strlen(inode_num) + 4 + 1;
                            char path[len];
                            memset(path, '\0', len);
                            sprintf(path, "%s/file-%s.jpg", argv[2], inode_num);
                            //file open
                            FILE *fp = fopen(path, "r");
                            if(fp == NULL) {
                                exit(0);
                            }
                            else {
                                //printf("Good to open file.");
                                fread(buffer, tempInNode->i_size, 1, fp);
                                //./length is 2, / length is 1
                                int len = 2 + strlen(argv[2]) + 1 + dir_entry->name_len + 1;
                                char path[len];
                                memset(path, '\0', len);
                                sprintf(path, "%s/%s", argv[2], new_name);
                                FILE *fp2 = fopen(path, "w+");
                                // printf("path : %s\n", path);
                                fwrite(buffer, tempInNode->i_size, 1, fp2);
                                fclose(fp2);
                            }
                            fclose(fp);
                        }
                    }
                }
                int checkFile;
                if ((new_length % 4) == 0){
                    checkFile = new_length;
                }
                else{
                    checkFile = new_length + 4 - (new_length % 4);
                }
                if(dir_entry->rec_len != (checkFile + 8)) {
                    offsetNumber = offsetNumber + checkFile + 8;
                }
                else { 
                    offsetNumber = offsetNumber + dir_entry->rec_len;	
                }
            }
        }
        free(inode);
    }
    close(fd);
}
