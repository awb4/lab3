#include <comp421/filesystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/hardware.h>
#include "message.h"

#define IPB BLOCKSIZE / INODESIZE

union {
    struct fs_header head;
    struct inode inode[IPB];
    char buf[BLOCKSIZE];
} *read_block;

struct open_file_list {
    int fd;
    short inum;
    int blocknum;
    int position;
    struct open_file_list *next;
};

struct free_fd_list {
    int fd;
    struct free_fd_list *next;
};

bool *block_bitmap;
bool *inode_bitmap;

int cur_fd;
int nb;
int ni;

void *message;
struct fs_header header;
struct open_file_list *open_files;
struct free_fd_list *free_fds;

void YFSOpen(void *m);
void YFSClose(void *m);
void YFSCreate(void *m);
void YFSRead(void *m);
void YFSWrite(void *m);
void YFSSeek(void *m);
void YFSLink(void *m);
void YFSUnlink(void *m);
void YFSMkDir(void *m);
void YFSRmDir(void *m);
void YFSChDir(void *m);
void YFSStat(void *m);
void YFSSync(void *m);
void YFSShutdown(void *m);

void YFSCreateMkDir(void *m, bool directory);

short ParseFileName(char *filename, short dir_inum); 
char *GetPathName(pid_t pid, char *pathname);
char *GetBufContents(pid_t pid, void *buf, int len);
short TraverseDirs(char *dir_name, short dir_inum);
short TraverseDirsHelper(char *dir_name, int curr_block);
void InsertOpenFile(struct open_file_list **wait, int fd, short inum);
int InsertOFDeluxe(short inum); 
void InsertFD(struct free_fd_list **wait, int fd);
int RemoveOpenFile(struct open_file_list **wait, int fd);
struct open_file_list *SearchOpenFile(struct open_file_list **wait, short inum);
int EditOpenFile(struct open_file_list **wait, int fd, int blocknum, int position);
int RemoveMinFD(struct free_fd_list **wait);
void MakeNewFile(struct inode *inode, bool directory);

int ReadFromBlock(int sector_num, void *buf);
int WriteToBlock(int sector_num, void *buf);
int WriteINum(int inum, struct inode node);

short GetFreeInode();
int GetFreeBlock();
struct dir_entry *CreateDirEntry(short inum, char *name);

int GetSectorNum(int inum);
int GetSectorPosition(int inum);
struct inode *GetInodeAt(short inum);

int AddToBlock(short inum, void *buf, int len);
int AddBlockToInode(struct inode *node, int block_num);

int GetTrueBlock(struct inode *node, int blocknum);

int CeilDiv(int a, int b);

int 
main(int argc, char **argv) 
{
    (void) argc;
    TracePrintf(0, "Size of messageSinglePath: %d\n", sizeof(struct messageSinglePath));
    TracePrintf(0, "Size of messageDoublePath: %d\n", sizeof(struct messageDoublePath));
    TracePrintf(0, "Size of messageFDSizeBuf: %d\n", sizeof(struct messageFDSizeBuf));
    TracePrintf(0, "Size of messageSeek: %d\n", sizeof(struct messageSeek));
    cur_fd = 0;
    if (Register(FILE_SERVER) == -1) {
        TracePrintf(0, "Register File Server: Freak the Fuck out\n");
        Exit(-1);
    }
    if (Fork() == 0) {
        Exec(argv[1], argv + 1);
    } else {
        if (ReadFromBlock(1, read_block) < 0) {
            TracePrintf(0, "ReadFromBlock: Freak the Fuck out\n");
            Exit(-1);
        }
        nb = read_block->head.num_blocks;
        ni = read_block->head.num_inodes;
        int maxfb =  nb - CeilDiv(ni + 1, IPB); 
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
            struct messageSinglePath *castMsg = (struct messageSinglePath *) message;
            switch (castMsg->type) {
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
                    // YFSSymLink(message);
                    break;
                case 9:
                    // readlink
                    // YFSReadLink(message);
                    break;
                case 10:
                    // mkdir
                    YFSMkDir(message);
                    break;
                case 11:
                    // rmdir
                    YFSRmDir(message);
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
            Reply(message, pid);
        }
    }
    return 0;
}

/* ---------------------------------------- YFS Server Functions ---------------------------------------- */

/**
 * Open a file
 * 
 * Expected message struct: messageSinglePath
 */
void
YFSOpen(void *m)
{
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point
    short inum = ParseFileName(pathname, msg->cd);
    struct inode *my_node = GetInodeAt(inum);
    if (my_node->type != INODE_REGULAR) {
        TracePrintf(0, "Tried to use Open on a directory\n");
        msg->retval = ERROR;
        return;
    }
    
    if (inum <= 0) {
        TracePrintf(0, "ParseFilename found bullshit\n");
        msg->retval = ERROR;
        return;
    }
    else {
        msg->retval = InsertOFDeluxe(inum);
    }
}

/**
 * Close
 * 
 * Expected message struct: messageFDSizeBuf
 */
void
YFSClose(void *m)
{
    struct messageFDSizeBuf *msg = (struct messageFDSizeBuf *) m;
    int fd = (int) msg->fd; // check if cast is correct
    InsertFD(&free_fds, fd);
    if (RemoveOpenFile(&open_files, fd) == -1) {
        msg->retval = ERROR;
    } 
    else {
        msg->retval = 0;
    }
}

/**
 * Create 
 * 
 * Expected message struct: messageSinglePath
 */
void
YFSCreate(void *m)
{
    YFSCreateMkDir(m, false);
}

/**
 * Read 
 * 
 * Expected message struct: messageFDSizeBuf
 */
void
YFSRead(void *m)
{
    struct messageFDSizeBuf *msg = (struct messageFDSizeBuf *) m;
    int fd = (int) msg->fd;
    int size = (int) msg->size;
    void *buf = msg->buf;
    //char *buf_contents = GetBufContents(msg->pid, msg->buf, size); // Free this bitch at some point
    (void) buf_contents;
    (void) fd;
    // Check if fd is in the list of free files. If not return error.
    // Read from the file starting at the current position
        // Reads 0 bytes if we're at the end of the file
        // Otherwise read min(size, bytes left in file)
    // If not error, update current position according to number of bytes read
    // If not error, based on data read, update the contents of buf accordingly
    // Returns the number of bytes read or ERROR
    // It is not an error to attempt to Read() from a file descriptor that is open on a directory
}

/**
 * Write 
 * 
 * Expected message struct: messageFDSizeBuf
 */
void
YFSWrite(struct message*msg)
{
    struct messageFDSizeBuf *msg = (struct messageFDSizeBuf *) m;
    int fd = (int) msg->fd;
    int size = (int) msg->size;   
    char *buf_contents = GetBufContents(msg->pid, msg->buf, size); // Free this bitch at some point
    (void) buf_contents;
    (void) fd;
}

/**
 * Seek 
 * 
 * Expected message struct: messageSeek
 */
void
YFSSeek(void *m)
{
    struct messageSeek *msg = (struct messageSeek *) m;

    int fd = (int) msg->fd;
    int offset = (int) msg->offset;
    int size = (int) msg->size;
    (void) fd;
    (void) size;
    (void) offset;
}

/**
 * Link
 * 
 * Expected message struct: messageDoublePath
 */
void
YFSLink(void *m)
{
    struct messageDoublePath *msg = (struct messageDoublePath *) m;

    char *oldname = GetPathName(msg->pid, msg->pathname1); // Free this bitch at some point
    char *newname = GetPathName(msg->pid, msg->pathname2); // Free this bitch at some point
    (void) oldname;
    (void) newname;
}

/**
 * Unlink
 * 
 * Expected message struct: messageSinglePath
 */ 
void 
YFSUnlink(void *m)
{
    struct messageSinglePath *msg = (struct messageSinglePath *) m;

    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point
    (void) pathname;
}

// void
// YFSSymLink(struct message msg)
// {
//    return 0; 
// }

/**
 * ReadLink 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 *     text[8:16] ptr to buf
 *     text[16:20] int len
 */
// void
// YFSReadLink(struct message msg)
// {
    
// }

/**
 * MkDir 
 * 
 * Expected message struct: messageSinglePath
 */
void
YFSMkDir(void *m)
{
    YFSCreateMkDir(m, true);
}

/**
 * RmDir 
 * 
 * Expected message struct: messageSinglePath
 */
void
YFSRmDir(void *m)
{
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point
    (void) pathname;
}

/**
 * ChDir 
 * 
 * Expected message struct: messageSinglePath
 */
void
YFSChDir(void *m)
{
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point
    (void) pathname;
}

/**
 * Stat 
 * 
 * Expected message struct: messageDoublePath
 */
void
YFSStat(void *m)
{
    struct messageDoublePath *msg = (struct messageDoublePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point
    (void) pathname;
}

/**
 * Sync 
 * 
 * Expected message struct: messageSinglePath
 */
void
YFSSync(void *m)
{
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    (void) msg;
}

/**
 * Shutdoen 
 * 
 * Expected message struct: messageSinglePath
 */
void
YFSShutdown(void *m)
{
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    (void) msg;
    Exit(-1);
}

/* ---------------------------------------- YFS Helper Functions ---------------------------------------- */
char *
GetPathName(pid_t pid, char *pathname) {
    char *name = malloc(MAXPATHNAMELEN);
    CopyFrom(pid, name, pathname, MAXPATHNAMELEN);
    return name;
}

char *
GetBufContents(pid_t pid, void *buf, int len) {
    char *contents = malloc(len);
    CopyFrom(pid, contents, buf, len);
    return contents;
}

/**
 * @brief Looks for the file specified by the pathname "filename" starting
 * at the directory with inum "dir_inum". Returns the inum of the desired
 * file on success, -1 otherwise.
 * 
 * @param filename the path to the desired file
 * @param dir_inum the inum of the directory at which to start looking for "filename"
 * @return the inum for file "filename"
 */
short
ParseFileName(char *filename, short dir_inum) 
{  
    char *new_filename = MakeNullTerminated(filename, MAXPATHNAMELEN);
    
    char *token = strtok(new_filename, "/");
    short inum = TraverseDirs(token, dir_inum);
    while (token != 0) {
        token = strtok(0, "/");
        inum = TraverseDirs(token, inum);
    }
    free(new_filename);
    return inum;
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
    int sector_num = GetSectorNum(dir_inum);
    struct inode *first_inodes = malloc(SECTORSIZE);
    if (ReadFromBlock(sector_num, first_inodes) != 0) {
        TracePrintf(0, "Freak the fuck out\n");
        Exit(-1);
    }

    // iterate over direct block
    int diff = GetSectorPosition(dir_inum);
    struct inode *root_inode = first_inodes + diff;
    int i;
    for (i = 0; i < NUM_DIRECT; i++) {
        int curr_block = root_inode->direct[i];

        if (curr_block == 0)
            break;

        short inum = TraverseDirsHelper(dir_name, curr_block);
        if (inum > 0) {
            free(first_inodes);
            return inum;
        }
    }

    // iterate over indirect block
    int indir_block = root_inode->indirect;
    if (indir_block != 0) {
        int ints_per_block = BLOCKSIZE / sizeof(int);
        int j;

        // read contents of indirect block
        int *block = malloc(BLOCKSIZE);
        if (ReadFromBlock(indir_block, block) < 0) {
            TracePrintf(0, "TraverseDirs: Freak the fuck out\n");
        }

        // iterate over every block number in the indirect block
        for (j = 0; j < ints_per_block; j++) {
            if (block[j] == 0)
                break;
            short inum = TraverseDirsHelper(dir_name, block[j]);
            if (inum > 0) {
                free(first_inodes);
                return inum;
            }
        }
    }
    free(first_inodes);
    return -1;
}

/**
 * @brief iterates over the directory entries in "curr_block" looking for a directory
 * with the same name as "dir_name". If a match is found, return the inum in the
 * directory entry. Otherwise return -1.
 * 
 * @param dir_name the directory  name we are looking for
 * @param curr_block the block in which to search
 * @return short the inum at the directory entry with name "dir_name"
 */
short
TraverseDirsHelper(char *dir_name, int curr_block)
{
    struct dir_entry *dir_entries = malloc(SECTORSIZE);
    if (curr_block == 0) {
        free(dir_entries);
        return -1;
    }    
        
    if (ReadFromBlock(curr_block, dir_entries) != 0) {
        TracePrintf(0, "Freak the fuck out\n");
        Exit(-1);
    }
        
    struct dir_entry *curr_entry = dir_entries;
    int entries_per_block = BLOCKSIZE / sizeof(struct dir_entry);
    int j;
    for (j = 0; j < entries_per_block; j++) {
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

//Put file into open file list with specific data as given by params
void
InsertOpenFile(struct open_file_list **wait, int fd, short inum)
{
	struct open_file_list *new = malloc(sizeof(struct open_file_list));
  
	new->fd = fd;
	new->inum = inum;
    new->next = NULL;
    new->blocknum = 0;
    new->position = 0;

    struct open_file_list *list = *wait;
    TracePrintf(0, "InsertNode: Outside loop 1\n");
    while (list->next != NULL) {
        list = list->next;
    }
    TracePrintf(0, "InsertNode: Outside loop 2\n");

    list->next = new;
}

//Removes given fd from open file list
int
RemoveOpenFile(struct open_file_list **wait, int fd)
{
	struct open_file_list *q = (*wait);
	struct open_file_list *prev;
    if (fd == -1) {
        TracePrintf(0, "RemoveOpenFile: Can't close directory\n");
        return -1;
    }
	if (q != NULL && q->fd == fd) {
		(*wait)->next = q->next;
		free(q);
		return (0);
	}
	while (q != NULL && q->fd != fd){
		prev = q;
		q = q->next;
	}

	if (q == NULL) {
		return (-1);
	}
	
	prev->next = q->next;
	free(q);
	return(0);
}

// Searches for a specific inum in open file list
struct open_file_list *
SearchOpenFile(struct open_file_list **wait, short inum)
{
	struct open_file_list *q = (*wait);
	if (q != NULL && q->inum == inum) {
		return (q);
	}
	while (q != NULL && q->inum != inum){
		q = q->next;
	}

	if (q == NULL) {
		return NULL;
	}
	return(q);
}

//Gives open file in open file list a given block num and position.
int
EditOpenFile(struct open_file_list **wait, int fd, int blocknum, int position) {
    struct open_file_list *q = (*wait);
	if (q != NULL && q->fd == fd) {
		q->blocknum = blocknum;
        q->position = position;
        free(q);
		return (0);
	}
	while (q != NULL && q->fd != fd){
		q = q->next;
	}

	if (q == NULL) {
		return (-1);
	}

	q->blocknum = blocknum;
    q->position = position;
    free(q);
	return(0);
}

//Puts fd into free fd list
void
InsertFD(struct free_fd_list **wait, int fd)
{
	struct free_fd_list *new = malloc(sizeof(struct free_fd_list));
  
	new->fd = fd;
    new->next = NULL;

    struct free_fd_list *list = *wait;
    TracePrintf(0, "InsertNode: Outside loop 1\n");
    while (list->next != NULL) {
        list = list->next;
    }
    TracePrintf(0, "InsertNode: Outside loop 2\n");

    list->next = new;
}

//Removes FD out of free fd list
int
RemoveMinFD(struct free_fd_list **wait)
{
	struct free_fd_list *q = (*wait);
	struct free_fd_list *prev;
    int min = INT_MAX;

    while (q != NULL) {
        if (q->fd < min) {
            min = q->fd;
        }
        q = q->next;
    }
    q = (*wait);
	if (q != NULL && q->fd == min) {
		(*wait)->next = q->next;
		free(q);
		return (0);
	}
	while (q != NULL && q->fd != min){
		prev = q;
		q = q->next;
	}

	if (q == NULL) {
		return (-2);
	}
	
	prev->next = q->next;
	free(q);
	return(min);
}

/**
 * Create/Mkdir hybrid
 * 
 * Expected message struct:
 *     text[0:8] ptr to pathname
 */
void 
YFSCreateMkDir(void *m, bool dir) 
{
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    struct inode *block;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point
    char *null_path = MakeNullTerminated(pathname, MAXPATHNAMELEN);
    char *directory;
    char *new_file;

    int i;
    int n = strlen(null_path);


    for (i = n - 1; i >= 0; i--) {
        if (null_path[i] == '/') {
            directory = malloc(i-1);
            new_file = malloc(n-i+1);

            // the lines below seems sus, copy manually instead?
            *directory = *null_path;
            *new_file = *(null_path + i + 1);
            break;
        }
    }

    short parent_inum = 1;
    if (i > 0) {
        parent_inum = ParseFileName(directory, msg->cd);
    } else if (i < 0) {
        directory = "";
        parent_inum = msg->cd;
        new_file = null_path;
    }
    
    if (parent_inum <= 0) {
        msg->retval = ERROR;
    }
    
    block = malloc(BLOCKSIZE);
    if (ReadFromBlock((int) (parent_inum / IPB + 1), block) == -1) {
        TracePrintf(0, "Freak the Fuck out\n");
        Exit(-1);
    }
    //int sector_num = GetSectorNum((int) parent_inum);
    int diff = GetSectorPosition((int) parent_inum);
    struct inode *cur_node = block + diff;
    if (cur_node->type == INODE_DIRECTORY) {
        //make new file
        struct inode *new_node;
        short child_inum;

        if ((child_inum = TraverseDirs(new_file, parent_inum)) <= 0) {
            // file we're creating does not have an inode yet
            child_inum = GetFreeInode();
            if (child_inum < 0) {
                msg->retval = ERROR;
                free(pathname);
                return;
            }
        } else if (dir) {
            // if we are trying to make a directory that alreay exists freak the fuck out
            msg->retval = ERROR;
            free(pathname);
            return;
        } else {
            new_node = GetInodeAt(child_inum);
        }

        // making a new directory or "creating" a file (existing or not)

        // create dir entry and add it to parent directory
        struct dir_entry *new_file_entry = CreateDirEntry(child_inum, new_file);
        AddToBlock(parent_inum, new_file_entry, sizeof(struct dir_entry));

        MakeNewFile(new_node, dir); // updates inode
        int openval = InsertOFDeluxe(child_inum);
        if (dir) {
            struct dir_entry *dot = CreateDirEntry(child_inum, ".");
            struct dir_entry *dotdot = CreateDirEntry(parent_inum, "..");
            struct dir_entry *blk = malloc(2 * sizeof(struct dir_entry)); //check in office hours, seems sus
            blk[0] = *dot;
            blk[1] = *dotdot;
            AddToBlock(child_inum, blk, 2*sizeof(struct dir_entry));
        }
        WriteINum(child_inum, *new_node);
        free(pathname);
        if (dir) {
            msg->retval = 0;
        } else {
            msg->retval = openval;
        }
    } else {
        TracePrintf(0, "Not Directory Freak the Fuck Out\n");
        free(pathname);
        msg->retval = ERROR;
    }
}

// Shoves data into empty inode struct
void
MakeNewFile(struct inode *node, bool directory) 
{
   
    if (directory) {
        node->type = INODE_DIRECTORY;
    } else {
        node->type = INODE_REGULAR;
    }
    int i;
    for (i = 0; i < NUM_DIRECT; i++){
        if (node->direct[i] != 0) {
            int position = node->direct[i];
            block_bitmap[position] = 1;
        }
        node->direct[i] = 0;
    }
    node->size = 0;
    node->nlink = 1;
    node->reuse = 0;
    node->indirect = 0;
}

//Gets inum of first free inode
short
GetFreeInode() 
{
    short i;
    for (i = 0; i < ni + 1; i++) {
        if (inode_bitmap[i]) {
            inode_bitmap[i] = 0;
            return i;
        }
    }
    TracePrintf(0, "No free nodes remaining.\n");
    return -1;
}

//Gets block num of first free block
int
GetFreeBlock() 
{
    int i;
    for (i = 0; i < nb + 1; i++) {
        if (block_bitmap[i]) {
            block_bitmap[0] = 1;
            return i;
        }
    }
    TracePrintf(0, "No free blocks remaining.\n");
    return -1;
}

//For readsector
int 
GetSectorNum(int inum) 
{
    return (inum / IPB) + 1;
}

//For readsector
int
GetSectorPosition(int inum) 
{
    int sector_num = GetSectorNum(inum);
    return (inum - ((sector_num - 1) * IPB));
}

//Creates new dir entry out of inum and name
struct dir_entry *
CreateDirEntry(short inum, char name[]) 
{
    struct dir_entry *entry = malloc(sizeof(struct dir_entry));
    entry->inum = inum;
    int i;
    for (i = 0; i < DIRNAMELEN; i++) {
        entry->name[i] = name[i];
        if (name[i] == '\0') {
            break;
        }
    }
    return entry;
}

/**
 * Read from cache. Otherwise, read the disk and write it into the cache.
 * 
 * */
int
ReadFromBlock(int sector_num, void *buf) 
{
    return ReadSector(sector_num, buf);
}
/**
 * Write to cache. Otherwise, read the disk and write it into the cache.
 * 
 * */
int
WriteToBlock(int sector_num, void *buf) 
{
    return WriteSector(sector_num, buf);
}

//Write to cache given only an inum and its inode
int
WriteINum(int inum, struct inode node) 
{
    int sector = GetSectorNum(inum);
    int position = GetSectorPosition(inum);
    struct inode *nodes = malloc(BLOCKSIZE);
    ReadFromBlock(sector, nodes);
    nodes[position] = node;
    return WriteToBlock(sector, nodes);
}

//Get an inode from an inum
struct inode *
GetInodeAt(short inum) 
{
    struct inode *blk = malloc(BLOCKSIZE);
    int sector = GetSectorNum((int) inum);
    ReadFromBlock(sector, blk);
    return blk + GetSectorPosition((int) inum);
}

//Add data to already open file (or directory)'s current block
int 
AddToBlock(short inum, void *buf, int len) 
{
    struct open_file_list *file = SearchOpenFile(&open_files, inum);
    if (file == NULL) {
        TracePrintf(0, "Add2Blk: File is not open\n");
        return (-1);
    }
    struct inode *node = GetInodeAt(inum);
    int blknum = file->blocknum;
    int pos = file->position;
    char *edit_blk = malloc(BLOCKSIZE);
    if (node->direct[0] == 0) {
        node->direct[0] = GetFreeBlock();
    }
    if (ReadFromBlock(node->direct[blknum], edit_blk) < 0) {
        TracePrintf(0, "Add2Blk: Reading from block Failed\n");
        return (-1);
    }
    if (pos + len < BLOCKSIZE) {
        int i;
        for (i = pos; i < pos + len; i++) {
            edit_blk[i] = *((char *) (buf + i - pos));
        }

        int write_num = GetTrueBlock(node, blknum);
        if (write_num <= 0) {
            TracePrintf(0, "Add2Blk: Wrong Block gotten by GetTrueBlock\n");
            return (-1);
        }

        WriteToBlock(write_num, edit_blk);
        EditOpenFile(&open_files, file->fd, blknum, pos + len);
    }
    else {
        int newpos;
        int i = pos;
        int write_num = GetTrueBlock(node, blknum);
        for (newpos = pos; newpos < pos + len; newpos++) {
            if (i >= BLOCKSIZE) {
                WriteToBlock(write_num, edit_blk);
                int newblk = GetFreeBlock();
                if (AddBlockToInode(node, newblk) == -1) {
                    TracePrintf(0, "Add2Blk: Couldn't add block to inode\n");
                    return (-1);
                }
                blknum++;
                write_num = newblk;
                i -= BLOCKSIZE;
                if (i != 0) {
                    TracePrintf(0, "Add2Blk: Something wrong with newpos loop\n");
                    return (-1);
                }
            }
            edit_blk[i] = *((char *) (buf + i - pos));
            i++;
        }
        WriteToBlock(write_num, edit_blk);
        EditOpenFile(&open_files, file->fd, blknum, i);
        WriteINum(inum, *node);
    }
    return 0;
}

//Add a new block to an inode
int
AddBlockToInode(struct inode *node, int block_num) 
{
    int i;
    for (i = 0; i < NUM_DIRECT; i++) {
        if (node->direct[i] == 0) {
            node->direct[i] = block_num;
        }
    }
    
    int *ind_block = malloc(BLOCKSIZE);
    if (node->indirect == 0) {
        int new_ind = GetFreeBlock();
        node->indirect = new_ind;
    } 
    ReadFromBlock(node->indirect, ind_block);
    int j;
    for (j = 0; j < (int) (BLOCKSIZE / sizeof(int)); j++) {
        if (ind_block[j] == 0) {
            ind_block[j] = block_num;
            break;
        }
    }
    if (i == BLOCKSIZE / sizeof(int)) {
        TracePrintf(0, "Add Block to Inode: somehow you've run out of space in the indirect block!\n");
        return (-1);
    }
    WriteToBlock(node->indirect, ind_block);
    return(0);
}

//Get the actual block number given the blocknum stored in its open file 
int
GetTrueBlock(struct inode *node, int blocknum) 
{
    if (blocknum < NUM_DIRECT){
        return node->direct[blocknum];
    }
    else {
        int *ind_blk = malloc(BLOCKSIZE);
        if (ReadFromBlock(node->indirect, ind_blk) < 0) {
            TracePrintf(0, "GetTrueBlock: Couldn't read from block");
            return -1;
        }
        return ind_blk[blocknum - NUM_DIRECT];
    }
}

//Insert an inum into the open file list and conduct extra checks
int
InsertOFDeluxe(short inum) 
{
    int new_fd;
    //THIS MEANS INODE IS DIRECTORY
    if (GetInodeAt(inum)->type == INODE_DIRECTORY) {
        new_fd = -1;
    } else {
        new_fd = RemoveMinFD(&free_fds);
        //THIS MEANS THERE IS NO REUSED FD
        if (new_fd == -2) {
            new_fd = cur_fd;
            cur_fd++;
        }
    }
    

    InsertOpenFile(&open_files, new_fd, inum);
    return new_fd;
}

int
CeilDiv(int a, int b) {
    if (a % b == 0)
        return a / b;
    else 
        return a / b + 1;
} 