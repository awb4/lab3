#include <comp421/filesystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <comp421/iolib.h>
#include <comp421/hardware.h>


#define DEFAULT_NUM_BLOCKS 128
#define DEFAULT_NUM_INODES 64

struct message {
    int type;
    char text[24];
};

struct message *message;
struct fs_header header;

int InitFileSystem(int num_blocks, int num_inodes);
void *entry(int num_blocks, int offset);


int 
main(char *argc, char **argv) 
{
    if (Fork() == 0) {
        Exec(argv[1], argv + 1);
    } else {
        int nb = DEFAULT_NUM_BLOCKS;
        int ni = DEFAULT_NUM_INODES;
        if (argc >= 3) {
            nb = (int) argv[1];
            ni = (int) argv[2];
        }
        if (InitFileSystem(nb, ni) == -1) {
            TracePrintf(0, "YFS Main: Freak the Fuck out");
            return -1;
        }
        while(1) {
            pid_t pid = Receive(*message);
            /* Construct and overwrite the message to reply */
            Reply(*message, pid);
        }
    }
}

int 
InitFileSystem(int num_blocks, int num_inodes)
{
    root = malloc(sizeof(struct file_tree));
    header.num_blocks = num_blocks;
    header.num_inodes = num_inodes;
    filesystem = malloc(BLOCKSIZE * num_blocks);
    int i;
    for (i = 0; i < ceil((num_inodes + 1) * INODESIZE / BLOCKSIZE) ; i++) {
        void 
    }
    /* Initialize fields of the root */
    cd = root;
}

void *
entry(int block_num, int inode_offset) {
    char *file = (char *) filesystem;
    return file + block_num * BLOCKSIZE + inode_offset * INODESIZE;
}

