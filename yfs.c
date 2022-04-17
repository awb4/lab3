#include <comp421/filesystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
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

/* ---------------------------------------- YFS Server Functions ---------------------------------------- */

/**
 * Open
 * 
 * Expected message struct:
 *     text[0:8] ptr to pathname
 */
int
YFSOpen(struct message msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

/**
 * Close
 * 
 * Expected message struct
 *     text[0:4] fd (file descriptor number)
 */
int
YFSClose(struct message msg)
{
    int fd = (int) message->text[0]; // check if cast is correct
}

/**
 * Create 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 */
int
YFSCreate(struct message msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

/**
 * Read 
 * 
 * Expected message struct 
 *     text[0:4] int fd (file descriptor)
 *     text[4:12] ptr to buf
 *     text[12:16] int size
 */
int
YFSRead(struct message msg)
{
    int fd = (int) message->text[0];
    int size = (int) message->text[12];
    char buf_contents = GetBufContents(msg, 4, len); // Free this bitch at some point
}

/**
 * Write 
 * 
 * Expected message struct 
 *     text[0:4] int fd (file descriptor)
 *     text[4:12] ptr to buf
 *     text[12:16] int size
 */
int
YFSWrite(struct message msg)
{
    int fd = (int) message->text[0];
    int size = (int) message->text[12];   
    char buf_contents = GetBufContents(msg, 4, len); // Free this bitch at some point
}

/**
 * Seek 
 * 
 * Expected message struct 
 *     text[0:4] int fd (file descriptor)
 *     text[4:8] int offset
 *     text[8:12] int whence
 */
int
YFSSeek(struct message msg)
{
    int fd = (int) message->text[0];
    int offset = (int) message->text[4];
    int size = (int) message->text[8];
}

/**
 * Link
 * 
 * Expected message struct:
 *     text[0:8] ptr to old name
 *     text[8:16] ptr to new name
 */
int
YFSLink(struct message msg)
{
    char *oldname = GetPathName(message, 0); // Free this bitch at some point
    char *newname = GetPathName(message, 8); // Free this bitch at some point
    
}

/**
 * Unlink
 * 
 * Expected message struct:
 *     text[0:8] ptr to pathname
 */ 
int 
YFSUnlink(struct message msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

int
YFSSymLink(struct message msg)
{
   return 0; 
}

/**
 * ReadLink 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 *     text[8:16] ptr to buf
 *     text[16:20] int len
 */
int
YFSReadLink(struct message msg)
{
    return 0;
}

/**
 * MkDir 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 */
int
YFSMkDir(struct message msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

/**
 * RmDir 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 */
int
YFSRmDir(struct message msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

/**
 * ChDir 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 */
int
YFSChDir(struct message msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

/**
 * Stat 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 *     text[8:16] ptr to struct Stat 
 */
int
YFSStat(struct message msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

/**
 * Sync 
 * 
 * Expected message struct
 *     text has no meaningful contents 
 */
int
YFSSync(struct message msg)
{
    
}

/**
 * Shutdoen 
 * 
 * Expected message struct 
 *     text has no meaningful contents 
 */
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
    return name;
}

char *
GetBufContents(struct message *msg, int text_pos, int len) {
    char *contents = malloc(len);
    char *src = (char *) message->text[text_pos]; 
    CopyFrom(message->pid, contents, src, len);
    return contents;
}

short inum
ParseFileName(char *filename) 
{  
    char *new_filename = MakeNullTerminated(filename, MAXPATHNAMELEN);
    
    char *token = strtok(new_filename, '/');
    while (token != 0) {
        token = strtok(0, '/');
    }
    free(new_filename);
}

/**
 * @brief returns a new char * with a null terminated version of the input string.
 * 
 * @param str the string to turn into a null terminated string
 * @param max_len the max len of the input string
 * @return char* a new char * with a null terminated version of the input string.
 */
char *
MakeNullTerminated(char *str, int max_len) {
    int i;
    for (i = 0; i < max_len; i++) {
        if (str[i] == '\0')
            break;
    }
    if (i == (max_len - 1) && str[i] != '\0') {
        char *new_str = malloc(max_len + 1);
        for (int j = 0; j < max_len; j++) {
            new_str[j] = str[j];
        }
        new_str[max_len] = '\0';
        return new_str;
    }
    char *new_str = malloc(i + 1);
    for (int j = 0; j <= i; j++) {
        new_str[j] = str[j];
    }
    return new_str;
}

/**
 * @brief 
 * 
 * @param dir_name a null terminated directory name
 * @param dir_inum an inode number
 * @return short the inum corresponding to dir_name
 */
short
TraverseDirs(char *dir_name, short dir_inum)
{
    int sector_num = (dir_inum / IPB) + 1;
    struct inode *first_inodes = malloc(SECTORSIZE);
    if (ReadSector(sector_num, first_inodes) != 0) {
        TracePrintf(0, "Freak the fuck out\n");
        Exit(-1);
    }

    // iterate over direct block
    int diff = dir_inum - ((sector_num - 1) * IPB)
    struct inode *root_inode = first_inodes + diff;
    for (int i = 0; i < NUM_DIRECT; i++) {
        int curr_block = root_inode->direct[i];

        if (curr_block == 0)
            break;

        short inum = TraverseDirsHelper(dir_name, curr_block);
        if (inum > 0) {
            free(first_inodes);
            return inum;
        }
    }

    int indir_block = root_inode->indirect;
    if (indir_block != 0) {
        int ints_per_block = BLOCKSIZE / sizeof(int);
        for (int j = 0; j < ints_per_block; j++) {
            if (j == 0)
                break;

            short inum = TraverseDirsHelper(dir_name, j);
            if (inum > 0) {
                free(first_inodes);
                return inum;
            }
        }
    }
    free(first_inodes);
    return -1;
}

short
TraverseDirsHelper(char *dir_name, int curr_block)
{
    struct dir_entry *dir_entries = malloc(SECTORSIZE);
    if (curr_block == 0) {
        free(dir_entry);
        return -1;
    }    
        
    if (ReadSector(curr_block, dir_entries) != 0) {
        TracePrintf(0, "Freak the fuck out\n");
        Exit(-1);
    }
        
    struct dir_entry *curr_entry = dir_entries;
    int entries_per_block = BLOCKSIZE / sizeof(struct dir_entry);
    for (int j = 0; j < entries_per_block; j++) {
        char *curr_dir_name = MakeNullTerminated(curr_entry->name, DIRNAMELEN);
        if (curr_dir_name == dir_name) {
            free(dir_entries);
            return (curr_entry->inum);
        }
        curr_entry++;
    }

    free(dir_entries);
    return -1;
}
