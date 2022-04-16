#include <comp421/filesystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <comp421/iolib.h>
#include <comp421/hardware.h>

#define IPB BLOCKSIZE / INODESIZE

bool *block_bitmap;
bool *inode_bitmap;

union {
    struct fs_header head;
    struct inode inode[IPB];
    char buf[BLOCKSIZE];
} *read_block;

struct message {
    int type;
    pid_t pid;
    char text[24];
};

struct message *message;
struct fs_header header;

int 
main(char *argc, char **argv) 
{
    if (Register(FILE_SERVER) == -1) {
        TracePrintf(0, "Register File Server: Freak the Fuck out\n");
        Exit(-1);
    }
    if (Fork() == 0) {
        Exec(argv[1], argv + 1);
    } else {
        if (ReadSector(1, read_block) < 0) {
            TracePrintf(0, "ReadSector: Freak the Fuck out\n");
            Exit(-1);
        }
        int nb = read_block->head.num_blocks;
        int ni = read_block->head.num_inodes;
        int maxfb =  nb - ceil((ni + 1) / IPB); 
        block_bitmap = malloc(sizeof(bool) * maxfb);
        inode_bitmap = malloc(sizeof(bool) * ni);
        
        int i;
        //Init free blocks
        for (i = 1; i < maxfb; i++) {
            block_bitmap[i] = true;
        }
        
        block_bitmap[0] = false;

        
        //Init free inodes
        for (i = 2; i < ni; i++) {
            inode_bitmap[i] = true;
        }

        inode_bitmap[0] = false;
        inode_bitmap[1] = false;
       
        while(1) {
            
            pid_t pid = Receive(message);
            switch (message->type) {
                case 0:
                    // open
                    YFSOpen(message);
                    break;  
                case 1:
                    // close 
                    YFSClose(message);
                    break;
                case 2: 
                    // create
                    YFSCreate(message);
                    break;
                case 3:
                    // read
                    YFSRead(message);
                    break;
                case 4:
                    // write
                    YFSWrite(message);
                    break;
                case 5:
                    // seek
                    YFSSeek(message);
                    break;
                case 6:
                    // link
                    YFSLink(message);
                    break;
                case 7:
                    // unlink
                    YFSUnlink(message);
                    break;
                case 8:
                    // symlink
                    YFSSymLink(message);
                    break;
                case 9:
                    // readlink
                    YFSReadLink(message);
                    break;
                case 10:
                    // mkdir
                    YFSMkDir(message);
                    break;
                case 11:
                    // rmdir
                    YFSRkDir(message);
                    break;
                case 12:
                    // chdir
                    YFSChDir(message);
                    break;
                case 13:
                    // stat
                    YFSStat(message);
                    break;
                case 14:
                    // sync
                    YFSSync(message);
                    break;
                case 15:
                    // shutdown
                    YFSShutdown(message);
                    break;
            }
            /* Construct and overwrite the message to reply */
            Reply(*message, pid);
        }
    }
}

void *
entry(int block_num, int inode_offset) {
    char *file = (char *) filesystem;
    return file + block_num * BLOCKSIZE + inode_offset * INODESIZE;
}

/* ---------------------------------------- YFS Server Functions ---------------------------------------- */

int
YFSOpen(struct message msg)
{

}

int
YFSClose(struct message msg)
{
    
}

int
YFSCreate(struct message msg)
{
    
}

int
YFSRead(struct message msg)
{
    
}

int
YFSWrite(struct message msg)
{
    
}

int
YFSSeek(struct message msg)
{
    
}

int
YFSLink(struct message msg)
{
    //text[0:8] = ptr to old name
    //text[8:16] = ptr to new name
    char *oldname = GetPathName(message, 0);
    char *newname = GetPathName(message, 8);
    
}

int 
YFSUnlink(struct message msg)
{
    char *oldname = GetPathName(message, 0); //text[0:8] = ptr to oldname
}

int
YFSSymLink(struct message msg)
{
    
}

int
YFSReadLink(struct message msg)
{
    
}

int
YFSMkDir(struct message msg)
{
    
}

int
YFSRmDir(struct message msg)
{
    
}

int
YFSChDir(struct message msg)
{
    
}

int
YFSStat(struct message msg)
{
    
}

int
YFSSync(struct message msg)
{
    
}

int
YFSShutdown(struct message msg)
{
    
}

/* ---------------------------------------- YFS Helper Functions ---------------------------------------- */
char *
GetPathName(struct message *msg, int text_pos) {
    char *name = malloc(MAXPATHNAMELEN);
                    
    char *src = (char *) message->text[text_pos];

    CopyFrom(message->pid, name, src, MAXPATHNAMELEN);
}