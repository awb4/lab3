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

struct block_cache {
    int counter;
    int lru[BLOCK_CACHESIZE];
    bool dirty_bits[BLOCK_CACHESIZE];
    short nums[BLOCK_CACHESIZE];
    char blocks[BLOCK_CACHESIZE][BLOCKSIZE];
};


struct inode_cache {
    int counter;
    int lru[INODE_CACHESIZE];
    bool dirty_bits[INODE_CACHESIZE];
    short nums[INODE_CACHESIZE];
    struct inode nodes[INODE_CACHESIZE];
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

struct fs_header header;
struct open_file_list *open_files;
struct free_fd_list *free_fds;

struct block_cache b_cache;
struct inode_cache i_cache;


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
short TraverseDirs(char *dir_name, short dir_inum, bool delete);
short TraverseDirsHelper(char *dir_name, int curr_block, int num_dir_entries, bool delete);
void InsertOpenFile(struct open_file_list **wait, int fd, short inum);
int InsertOFDeluxe(short inum); 
void InsertFD(struct free_fd_list **wait, int fd);
int RemoveOpenFile(struct open_file_list **wait, int fd);
struct open_file_list *SearchOpenFile(struct open_file_list **wait, short inum);
struct open_file_list *SearchByFD(struct open_file_list **wait, int fd);
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
int GetFromBlock(short inum, void *buf, int len);
int AddBlockToInode(struct inode *node, int block_num);

int GetTrueBlock(struct inode *node, int blocknum);

int CeilDiv(int a, int b);

bool CheckDir(struct inode *node);
bool CheckDirHelper(int curr_block, int num_dir_entries);
void FreeFileBlocks(struct inode *node);

int InsertBlockCache(short idx, void *block, bool dirty);
int InsertInodeCache(short idx, struct inode node, bool dirty);
int GetBlockCache(short index, void *buf);
int GetInodeCache(short index, struct inode *buf);

int 
main(int argc, char **argv) 
{
    (void) argc;
    message = malloc(sizeof(struct messageSinglePath));
    TracePrintf(0, "Message: %p\n", message);
    cur_fd = 1;
    read_block = malloc(BLOCKSIZE);
    if (Register(FILE_SERVER) == -1) {
        TracePrintf(0, "Register File Server: Freak the Fuck out\n");
        Exit(-1);
    }
    b_cache.counter = 0;
    i_cache.counter = 0;
    if (Fork() == 0) {
        Exec(argv[1], argv + 1);
    } else {
        InsertOFDeluxe(1);
        if (ReadFromBlock(1, read_block) < 0) {
            TracePrintf(0, "ReadFromBlock: Freak the Fuck out\n");
            Exit(-1);
        }
        nb = read_block->head.num_blocks;
        ni = read_block->head.num_inodes;
        // int maxfb =  nb - CeilDiv(ni + 1, IPB); 
        block_bitmap = malloc(sizeof(bool) * nb);
        inode_bitmap = malloc(sizeof(bool) * ni);
        
        int i;
        //Init free blocks
        TracePrintf(0, "Initializing Free Blocks\n");
        for (i = 0; i < CeilDiv(ni + 1, IPB) + 2; i++) {
            block_bitmap[i] = false;
        }
        for (i = CeilDiv(ni + 1, IPB) + 2; i < nb; i++) {
            block_bitmap[i] = true;
        }


        TracePrintf(0, "Initializing Free inodes\n");
        //Init free inodes
        for (i = 2; i < ni; i++) {
            inode_bitmap[i] = true;
        }

        inode_bitmap[0] = false;
        inode_bitmap[1] = false;
        while(1) {
            TracePrintf(0, "--------------------Getting Message--------------------\n");
            pid_t pid = Receive(message);
            if (pid == 0) {
                TracePrintf(0, "Receive Call Breaks Deadlock! Fuck!\n");
                return -1;
            }
            TracePrintf(0, "Received Message\n");
            TracePrintf(0, "Message Type: %d\n", message->type);
            TracePrintf(0, "Message Pid: %d\n", message->pid);
            switch (message->type) {
                case 0:
                    // open
                    TracePrintf(0, "0: Open\n");
                    YFSOpen(message);
                    break;  
                case 1:
                    // close 
                    TracePrintf(0, "1: Close\n");
                    YFSClose(message);
                    break;
                case 2: 
                    // create
                    TracePrintf(0, "2: Create\n");
                    YFSCreate(message);
                    break;
                case 3:
                    // read
                    TracePrintf(0, "3: Read\n");
                    YFSRead(message);
                    break;
                case 4:
                    // write
                    TracePrintf(0, "4: Write\n");
                    YFSWrite(message);
                    break;
                case 5:
                    // seek
                    TracePrintf(0, "5: Seek\n");
                    YFSSeek(message);
                    break;
                case 6:
                    // link
                    TracePrintf(0, "6: Link\n");
                    YFSLink(message);
                    break;
                case 7:
                    // unlink
                    TracePrintf(0, "7: Unlink\n");
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
                    TracePrintf(0, "10: Mkdir\n");
                    YFSMkDir(message);
                    break;
                case 11:
                    // rmdir
                    TracePrintf(0, "11: Rmdir\n");
                    YFSRmDir(message);
                    break;
                case 12:
                    // chdir
                    TracePrintf(0, "12: Chdir\n");
                    YFSChDir(message);
                    break;
                case 13:
                    // stat
                    TracePrintf(0, "13: Stat\n");
                    YFSStat(message);
                    break;
                case 14:
                    // sync
                    TracePrintf(0, "14: Sync\n");
                    YFSSync(message);
                    break;
                case 15:
                    // shutdown
                    TracePrintf(0, "15: Shutdown\n");
                    YFSShutdown(message);
                    break;
            }
            /* Construct and overwrite the message to reply */
            if (Reply(message, pid) != 0) {
                TracePrintf(0, "Reply did not return 0, freak the fuck out!\n");
                Exit(-1);
            }
            TracePrintf(0, "--------------------Replied successfully--------------------\n");
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
    TracePrintf(0, "YFSOpen Entry\n");
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point
    TracePrintf(0, "YFSOpen: opening Path %s at cd %d\n", pathname, msg->cd);
    short inum = ParseFileName(pathname, msg->cd);
    if (inum <= 0) {
        TracePrintf(0, "YFSOpen: (ERROR) parseFilename found bullshit\n");
        msg->retval = ERROR;
        return;
    } else {
        struct inode *my_node = GetInodeAt(inum);
        TracePrintf(0, "YFSOpen: (ERROR) opening Inode %d at pathname %s\n", inum, pathname);
        if (my_node->type != INODE_REGULAR) {
            TracePrintf(0, "YFSOpen: (ERROR) tried to use Open on a directory\n");
            msg->retval = ERROR;
            return;
        }
        msg->retval = InsertOFDeluxe(inum);
    }
    TracePrintf(0, "YFSOpen Exit\n");
}

/**
 * Close
 * 
 * Expected message struct: messageFDSizeBuf
 */
void
YFSClose(void *m)
{
    TracePrintf(0, "YFSClose Entry\n");
    struct messageFDSizeBuf *msg = (struct messageFDSizeBuf *) m;
    int fd = (int) msg->fd; // check if cast is correct
    InsertFD(&free_fds, fd);
    if (RemoveOpenFile(&open_files, fd) == -1) {
        TracePrintf(0, "YFSClose: (ERROR) RemoveOpenFile failed\n");
        msg->retval = ERROR;
    } else {
        msg->retval = 0;
    }
    TracePrintf(0, "YFSClose Exit\n");
}

/**
 * Create 
 * 
 * Expected message struct: messageSinglePath
 */
void
YFSCreate(void *m)
{
    TracePrintf(0, "YFSCreate \n");
    TracePrintf(0, "YFSCreate open_files: %p\n", open_files);
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
    TracePrintf(0, "YFSRead Entry\n");
    struct messageFDSizeBuf *msg = (struct messageFDSizeBuf *) m;
    int fd = (int) msg->fd;
    int size = (int) msg->size;
    //void *buf = msg->buf;
    if (fd == -1) {
        TracePrintf(0, "YFSRead: (ERROR) cannot write to directory\n");
        msg->retval = ERROR;
        return;
    } else {
        struct open_file_list *file = SearchByFD(&open_files, fd);
        if (file == NULL) {
            TracePrintf(0, "YFSRead: (ERROR) file not in open_files list\n");
            msg->retval = ERROR;
            return;
        }
        char *my_buf = malloc(size);
        int gfb = GetFromBlock(file->inum, my_buf, size);
        
        if (CopyTo(msg->pid, msg->buf, my_buf, gfb) < 0) {
            TracePrintf(0, "YFSRead: (ERROR) CopyTo failed\n");
            msg->retval = ERROR;
            return;
        }
        msg->retval = gfb;
    }
    TracePrintf(0, "YFSRead Exit\n");
}

/**
 * Write 
 * 
 * Expected message struct: messageFDSizeBuf
 */
void
YFSWrite(void *m)
{
    TracePrintf(0, "YFSWrite Entry\n");
    struct messageFDSizeBuf *msg = (struct messageFDSizeBuf *) m;
    int fd = (int) msg->fd;
    int size = (int) msg->size;   
    char *buf_contents = GetBufContents(msg->pid, msg->buf, size); // Free this bitch at some point
    if (fd == -1) {
        TracePrintf(0, "YFSWrite: (ERROR) cannot write to directory\n");
        msg->retval = ERROR;
        return;
    } else {
        struct open_file_list *file = SearchByFD(&open_files, fd);
        if (file == NULL) {
            TracePrintf(0, "YFSWrite: (ERROR) file not in open_files list\n");
            msg->retval = ERROR;
            return;
        }
        if (GetInodeAt(file->inum)->type == INODE_DIRECTORY) {
            TracePrintf(0, "YFSWrite: (ERROR) can't write to directory\n");
            msg->retval = ERROR;
            return;
        }
        msg->retval = AddToBlock(file->inum, buf_contents, size);
        TracePrintf(0, "YFSWrite: returning %d\n", msg->retval);
    }
    TracePrintf(0, "YFSWrite Exit\n");
}

/**
 * Seek 
 * 
 * Expected message struct: messageSeek
 */
void
YFSSeek(void *m)
{
    TracePrintf(0, "YFSSeek Entry\n");
    struct messageSeek *msg = (struct messageSeek *) m;

    int fd = (int) msg->fd;
    int offset = (int) msg->offset;
    int whence = (int) msg->whence;
    struct open_file_list *file = SearchByFD(&open_files, fd);
    struct inode *node = GetInodeAt(file->inum);
    int size = node->size;
    
    int curpos;
    if (whence == SEEK_SET) {
        TracePrintf(0, "YFSSeek set\n");
        curpos = offset;
    } else if (whence == SEEK_CUR) {
        TracePrintf(0, "YFSSeek cur\n");
        curpos = file->position + BLOCKSIZE * file->blocknum;
        curpos = curpos + offset;
    } else if (whence == SEEK_END) {
        TracePrintf(0, "YFSSeek end\n");
        curpos = offset + size;
    } else {
        TracePrintf(0, "YFSSeek: (ERROR) did not get SEEK_SET/SEEK_CUR/SEEK_END\n");
        //file->position = 0;
        //file->blocknum = 0;
        msg->retval = ERROR;
        return;
    }
    if (curpos > 0) {
        if (curpos > size) {
            node->size = curpos;
            if (WriteINum(file->inum, *node) < 0) {
                TracePrintf(0, "YFSSeek: (ERROR) WriteInum failed\n");
                msg->retval = ERROR;
                return;
            }
        }
        file->position = curpos % BLOCKSIZE;
        file->blocknum = curpos / BLOCKSIZE; 
    } else {
        file->position = 0;
        file->blocknum = 0;
    } 
    TracePrintf(0, "YFSSeek Exit\n");
    msg->retval = 0;
    return;
}

/**
 * Link
 * 
 * Expected message struct: messageDoublePath
 */
void
YFSLink(void *m)
{
    TracePrintf(0, "YFSLink Entry\n");
    struct messageDoublePath *msg = (struct messageDoublePath *) m;

    char *oldname = GetPathName(msg->pid, msg->pathname1); // Free this bitch at some point
    char *newname = GetPathName(msg->pid, msg->pathname2); // Free this bitch at some point

    char *old_null_path = MakeNullTerminated(oldname, MAXPATHNAMELEN);
    char *olddirectory;
    char *oldfilename;
    int i;
    int n = strlen(old_null_path);
    TracePrintf(0, "YFSLink old_null_path length: %d\n", n);
    TracePrintf(0, "YFSLink old_null_path: %s\n", old_null_path);
    for (i = n - 1; i >= 0; i--) {
        if (old_null_path[i] == '/') {
            TracePrintf(0, "YFSLink loop entry %d\n", i);
            oldfilename = malloc(n-i+1);
            int k;
            if (i > 0) {
                olddirectory = malloc(i + 1);
                for (k = 0; k < i; k++){
                    olddirectory[k] = old_null_path[k];
                }
                olddirectory[i] = '\0';
            } else {
                olddirectory = "";
            }
            
            for (k = 0; k < n - i + 1; k++){
                oldfilename[k] = old_null_path[k + i + 1];
            }
            // the lines below seems sus, copy manually instead?
            break;
        }
    }
    short old_parent_inum = 1;
    if (i > 0) {
        old_parent_inum = ParseFileName(olddirectory, msg->cd1);
    } else if (i < 0) {
        olddirectory = "";
        old_parent_inum = msg->cd1;
        oldfilename = old_null_path;
    }
    
    if (old_parent_inum <= 0) {
        TracePrintf(0, "YFSLink: (ERROR) old_parent_inum <= 0\n");
        msg->retval = ERROR;
        return;
    }
    short oldfile_inum = ParseFileName(oldfilename, old_parent_inum);
    TracePrintf(0, "YFSLink olddirectory: %s\n", olddirectory);
    TracePrintf(0, "YFSLink oldfilename: %s\n", oldfilename);
    TracePrintf(0, "YFSLink old_parent_inum: %d\n", old_parent_inum);
    TracePrintf(0, "YFSLink oldfile_inum: %d\n", oldfile_inum);

 
    char *new_null_path = MakeNullTerminated(newname, MAXPATHNAMELEN);
    char *newdirectory;
    char *newfilename;
    n = strlen(new_null_path);
    TracePrintf(0, "YFSLink new_null_path length: %d\n", n);
    TracePrintf(0, "YFSLink new_null_path: %s\n", new_null_path);
    for (i = n - 1; i >= 0; i--) {
        if (new_null_path[i] == '/') {
            TracePrintf(0, "YFSLink loop entry %d\n", i);
            newfilename = malloc(n-i+1);
            int k;
            if (i > 0) {
                newdirectory = malloc(i + 1);
                for (k = 0; k < i; k++){
                    newdirectory[k] = new_null_path[k];
                }
                newdirectory[i] = '\0';
            } else {
                newdirectory = "";
            }
            
            for (k = 0; k < n - i + 1; k++){
                newfilename[k] = new_null_path[k + i + 1];
            }
            // the lines below seems sus, copy manually instead?
            break;
        }
    }
    short new_parent_inum = 1;
    if (i > 0) {
        new_parent_inum = ParseFileName(newdirectory, msg->cd2);
    } else if (i < 0) {
        newdirectory = "";
        new_parent_inum = msg->cd2;
        newfilename = new_null_path;
    }
    
    if (new_parent_inum <= 0) {
        TracePrintf(0, "YFSLink: (ERROR) new_parent_inum <= 0\n");
        msg->retval = ERROR;
        return;
    }
    short newfile_inum = ParseFileName(newfilename, new_parent_inum);
    TracePrintf(0, "YFSLink newdirectory: %s\n", newdirectory);
    TracePrintf(0, "YFSLink newfilename: %s\n", newfilename);
    TracePrintf(0, "YFSLink new_parent_inum: %d\n", new_parent_inum);
    TracePrintf(0, "YFSLink newfile_inum: %d\n", newfile_inum);
    
    struct inode *cur_node = GetInodeAt(new_parent_inum);
    TracePrintf(0, "Parent type: %d\n", cur_node->type);
    if (cur_node->type == INODE_DIRECTORY) {
        short child_inum;
        if ((child_inum = TraverseDirs(newfilename, new_parent_inum, false)) <= 0) {
            // file we're creating does not have an inode yet
            TracePrintf(0, "YFSLink: (success) newpathname does not already exist.\n");
            child_inum = oldfile_inum;
            if (child_inum < 0) {
                TracePrintf(0, "YFSLink: (ERROR) oldfile_inum is < 0\n");
                msg->retval = ERROR;
                return;
            }
        } else {
            // if new file already exists freak the fuck out
            TracePrintf(0, "YFSLink: (ERROR) newpathname already exists.\n");
            msg->retval = ERROR;
            return;
        }
        // create dir entry and add it to parent directory
        struct dir_entry *new_file_entry = CreateDirEntry(child_inum, newfilename);
        // AddToBlock(old_parent_inum, new_file_entry, sizeof(struct dir_entry));
        AddToBlock(new_parent_inum, new_file_entry, sizeof(struct dir_entry));

        // Update inode (child_inum == oldfile_inum) with new number of link
        struct inode *new_node;
        new_node = GetInodeAt(child_inum);
        new_node->nlink++;
        TracePrintf(0, "YFSLink new_node link: %d\n", new_node->nlink);
        WriteINum(child_inum, *new_node);
        msg->retval = 0;
    } else {
        TracePrintf(0, "YFSLink: (ERROR) not directory freak the fuck out\n");
        msg->retval = ERROR;
    }
    TracePrintf(0, "YFSLink Exit\n");
}

/**
 * Unlink
 * 
 * Expected message struct: messageSinglePath
 */ 
void 
YFSUnlink(void *m)
{
    TracePrintf(0, "YFSUnlink Entry\n");

    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point

    // Get inode/inum/name of parent directory and the file to be unlinked
    char *null_path = MakeNullTerminated(pathname, MAXPATHNAMELEN);
    char *directory;
    char *file;
    int i;
    int n = strlen(null_path);
    TracePrintf(0, "YFSUnlink null path length: %d\n", n);
    TracePrintf(0, "YFSUnlink finding last slash\n");
    TracePrintf(0, "YFSUnlink pathname: %s\n", null_path);

    for (i = n - 1; i >= 0; i--) {
        if (null_path[i] == '/') {
            TracePrintf(0, "YFSUnlink loop entry %d\n", i);
            file = malloc(n-i+1);
            int k;
            if (i > 0) {
                directory = malloc(i + 1);
                for (k = 0; k < i; k++){
                    directory[k] = null_path[k];
                }
                directory[i] = '\0';
            } else {
                directory = "";
            }
            
            for (k = 0; k < n - i + 1; k++){
                file[k] = null_path[k + i + 1];
            }
            break;
        }
    }

    short parent_inum = 1;
    if (i > 0) {
        parent_inum = ParseFileName(directory, msg->cd);
    } else if (i < 0) {
        directory = "";
        parent_inum = msg->cd;
        file = null_path;
    }
    
    if (parent_inum <= 0) {
        TracePrintf(0, "YFSUnlink: (ERROR) parent_inum <= 0\n");
        // free(pathname);
        // free(null_path);
        // free(file);
        msg->retval = ERROR;
        return;
    }
    TracePrintf(0, "YFSUnlink directory: %s\n", directory);
    TracePrintf(0, "YFSUnlink file: %s\n", file);
    TracePrintf(0, "YFSUnlink parent_inum: %d\n", parent_inum);

    short child_inum = ParseFileName(file, parent_inum);
    if (child_inum < 0) {
        TracePrintf(0, "YFSUnlink: (ERROR) child_inum < 0\n");
        // free(pathname);
        // free(null_path);
        // free(file);
        msg->retval = ERROR;
        return;
    }

    struct inode *child_node = GetInodeAt(child_inum);
    if (child_node->type == INODE_DIRECTORY) {
        TracePrintf(0, "YFSUnlink: (ERROR) cannot unlink a directory\n");
        // free(pathname);
        // free(null_path);
        // free(file);
        msg->retval = ERROR;
        return;
    }

    child_node->nlink--;
    // Free the inode and its blocks
    if (child_node->nlink == 0) {
        TracePrintf(0, "YFSUnlink: child_node has zero links, freeing inode and blocks\n");
        inode_bitmap[child_inum] = true;
        FreeFileBlocks(child_node); // maybe not necessary?
    }

    // Delete the directory entry for this file using TraverseDirs
    TraverseDirs(MakeNullTerminated(file, DIRNAMELEN), parent_inum, true);

    msg->retval = 0;
    // free(pathname);
    // free(null_path);
    // free(file);
    TracePrintf(0, "YFSUnlink Exit\n");
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
    TracePrintf(0, "YFSRmDir Entry\n");
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point
    
    // Get inode/inum/name of parent directory and the directory to be deleted (child_directory)
    char *null_path = MakeNullTerminated(pathname, MAXPATHNAMELEN);
    char *parent_directory;
    char *child_directory;
    int i;
    int n = strlen(null_path);
    for (i = n - 1; i >= 0; i--) {
        if (null_path[i] == '/') {
            child_directory = malloc(n-i+1);
            int k;
            if (i > 0) {
                parent_directory = malloc(i + 1);
                for (k = 0; k < i; k++){
                    parent_directory[k] = null_path[k];
                }
                parent_directory[i] = '\0';
            } else {
                parent_directory = "";
            }
            
            for (k = 0; k < n - i + 1; k++){
                child_directory[k] = null_path[k + i + 1];
            }
            // the lines below seems sus, copy manually instead?
            break;
        }
    }

    short parent_inum = 1;
    if (i > 0) {
        parent_inum = ParseFileName(parent_directory, msg->cd);
    } else if (i < 0) {
        parent_directory = "";
        parent_inum = msg->cd;
        child_directory = null_path;
    }
    short child_dir_inum = ParseFileName(child_directory, parent_inum);
    TracePrintf(0, "YFSRmDir parent_directory: %s\n", parent_directory);
    TracePrintf(0, "YFSRmDir child_directory: %s\n", child_directory);
    TracePrintf(0, "YFSRmDir parent_inum: %d\n", parent_inum);
    TracePrintf(0, "YFSRmDir child_dir_inum: %d\n", child_dir_inum);

    if (child_dir_inum <= 0) {
        TracePrintf(0, "YFSRmDir: (ERROR) child_dir_inum <= 0\n");
        msg->retval = ERROR;
        // free(pathname);
        // free(null_path);
        // free(child_directory);
        return;
    } else if (child_dir_inum == 1) {
        TracePrintf(0, "YFSRmDir: (ERROR) can't remove root directory\n");
        msg->retval = ERROR;
        // free(pathname);
        // free(null_path);
        // free(child_directory);
        return;
    } else {
        struct inode *node = GetInodeAt(child_dir_inum);
        if (node->type != INODE_DIRECTORY) {
            TracePrintf(0, "YFSRmDir: (ERROR) can't remove non directory failure\n");
            msg->retval = ERROR;
            // free(pathname);
            // free(null_path);
            // free(child_directory);
            return;
        }
        
        if (!CheckDir(node)) {
            TracePrintf(0, "YFSRmDir: (ERROR) chosen directory is not empty\n");
            msg->retval = ERROR;
            // free(pathname);
            // free(null_path);
            // free(child_directory);
            return;
        }
        // Remove the entry corresponding to new_file
        TraverseDirs(child_directory, parent_inum, true);
        FreeFileBlocks(node);
        inode_bitmap[child_dir_inum] = 1;
        msg->retval = 0;
        // free(pathname);
        // free(null_path);
        // free(child_directory);
        TracePrintf(0, "YFSRmDir Exit\n");
    }
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
    msg->retval = ParseFileName(pathname, msg->cd);
    // free(pathname);
}

/**
 * Stat 
 * 
 * Expected message struct: messageDoublePath
 */
void
YFSStat(void *m)
{
    TracePrintf(0, "YFSStat Entry\n");
    struct messageDoublePath *msg = (struct messageDoublePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname1); // Free this bitch at some point

    // Get inode/inum/name of the parent directory and the file we want
    char *null_path = MakeNullTerminated(pathname, MAXPATHNAMELEN);
    char *parent_directory;
    char *file_path;

    int i;
    int n = strlen(null_path);
    for (i = n - 1; i >= 0; i--) {
        if (null_path[i] == '/') {
            file_path = malloc(n-i+1);
            int k;
            if (i > 0) {
                parent_directory = malloc(i + 1);
                for (k = 0; k < i; k++){
                    parent_directory[k] = null_path[k];
                }
                parent_directory[i] = '\0';
            } else {
                parent_directory = "";
            }
            
            for (k = 0; k < n - i + 1; k++){
                file_path[k] = null_path[k + i + 1];
            }
            // the lines below seems sus, copy manually instead?
            break;
        }
    }
    short parent_inum = 1;
    if (i > 0) {
        parent_inum = ParseFileName(parent_directory, msg->cd1);
    } else if (i < 0) {
        parent_directory = "";
        parent_inum = msg->cd1;
        file_path = null_path;
    }
    short file_inum = ParseFileName(file_path, parent_inum);
    TracePrintf(0, "YFSStat parent_directory: %s\n", parent_directory);
    TracePrintf(0, "YFSStat child_directory: %s\n", file_path);
    TracePrintf(0, "YFSStat parent_inum: %d\n", parent_inum);
    TracePrintf(0, "YFSStat file_inum: %d\n", file_inum);
    if (file_inum <= 0) {
        TracePrintf(0, "YFSStat: (ERROR) file_inum <= 0");
        msg->retval = ERROR;
        // free(pathname);
        // free(null_path);
        // free(file_path);
        return;
    }
    struct inode *my_node = GetInodeAt(file_inum);

    struct Stat *my_stat = malloc(sizeof(struct Stat));
    my_stat->inum = file_inum;
    my_stat->type = my_node->type;
    my_stat->size = my_node->size;
    my_stat->nlink = my_node->nlink;

    if (CopyTo(msg->pid, msg->pathname2, my_stat, sizeof(struct Stat)) < 0) {
        TracePrintf(0, "YFSStat: (ERROR) CopyTo failed\n");
        msg->retval = ERROR;
        // free(pathname);
        // free(null_path);
        // free(file_path);
        return;
    }
    msg->retval = 0;
    // free(pathname);
    // free(null_path);
    // free(file_path);
    TracePrintf(0, "YFSStat Exit\n");
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
    //DO DIRTY BIT SHIT
    printf("System is shutting down due to user request\n");
    Exit(0);
}

/* ---------------------------------------- YFS Helper Functions ---------------------------------------- */
char *
GetPathName(pid_t pid, char *pathname) {
    TracePrintf(0, "GetPathName Entry\n");
    char *name = malloc(MAXPATHNAMELEN);
    CopyFrom(pid, name, pathname, MAXPATHNAMELEN);
    TracePrintf(0, "GetPathName Exit\n");
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
    TracePrintf(0, "ParseFileName Entry\n");
    char *new_filename = MakeNullTerminated(filename, MAXPATHNAMELEN);
    TracePrintf(0, "ParseFileName new_filename: %s\n", new_filename);
    TracePrintf(0, "ParseFileName dir_inum: %d\n", dir_inum);
    
    char *token = strtok(new_filename, "/");
    short inum;
    if (strcmp(token, "") == 0) {
        inum = 1;
    }
    TracePrintf(0, "ParseFileName Entering While loop\n");
    while (token != NULL) {
        int temp = TraverseDirs(token, dir_inum, false);
        if (temp < 0) {
            TracePrintf(0, "ParseFileName couldn't find file in inum\n");
            TracePrintf(0, "ParseFileName Exit\n");
            
            // struct dir_entry *buf = (struct dir_entry *) b_cache.blocks[8];
            // int i;
            // for (i = 0; i < 3; i++) {
            //     TracePrintf(0, "in pfn block 8 position %d, entry %s\n", i, (buf + i)->name);
            // }
            TracePrintf(0, "ParseFileName Exit\n");
            return -1;
        }
        inum = temp;
        token = strtok(0, "/");
    }
    //free(new_filename);
    // struct dir_entry *buf = (struct dir_entry *) b_cache.blocks[8];
    // int i;
    // for (i = 0; i < 3; i++) {
    //     TracePrintf(0, "prior to pfn exit block 8 position %d, entry %s\n", i, (buf + i)->name);
    // }
    TracePrintf(0, "ParseFileName Exit\n");
    return inum;
}

/**
 * @brief looks for dir_name in the directory corresponding to inode dir_inum 
 * 
 * @param dir_name a directory name
 * @param dir_inum an inode number
 * @param delete, should the entry with dir_name in dir_inum be deleted? (If found)
 * @return short the inum corresponding to dir_name
 */
short
TraverseDirs(char *dir_name, short dir_inum, bool delete)
{
    TracePrintf(0, "TraverseDirs Entry\n");
    TracePrintf(0, "Traversing path %s at inum %d\n", dir_name, dir_inum);

    // struct dir_entry *buf = (struct dir_entry *) b_cache.blocks[8];
    // int j;
    // for (j = 0; j< 3; j++) {
    //     TracePrintf(0, "TraverseDirs block 8 position %d, entry %s\n", j, (buf+j)->name);
    // }
    TracePrintf(0, "ParseFileName Exit\n");
    char *dir_name_copy = MakeNullTerminated(dir_name, DIRNAMELEN);
    struct inode *root_inode = GetInodeAt(dir_inum);
    TracePrintf(0, "TraverseDirs inode type: %d\n", root_inode->type);
    if (root_inode->type != INODE_DIRECTORY) {
        TracePrintf(0, "TraverseDirs dir_inum corresponds to a file not a directory\n");
        return -1;
    }

    int size_remaining = root_inode->size;
    int i;
    for (i = 0; i < NUM_DIRECT; i++) {
        TracePrintf(0, "TraverseDirs traversing direct block %d\n", i);
        int curr_block = root_inode->direct[i];

        if (curr_block == 0)
            break;

        int num_dir_entries = size_remaining;
        TracePrintf(0, "Size Remaining :%d\n", size_remaining);
        if (size_remaining > BLOCKSIZE)
            num_dir_entries = BLOCKSIZE;
        num_dir_entries = num_dir_entries / sizeof(struct dir_entry);

        short inum = TraverseDirsHelper(dir_name_copy, curr_block, num_dir_entries, delete);
        TracePrintf(0, "TraverseDirs found inum %d\n", inum);
        if (inum > 0) {
            return inum;
        }
        size_remaining -= BLOCKSIZE;
    }

    // iterate over indirect block
    int indir_block = root_inode->indirect;
    if (indir_block != 0) {
        int ints_per_block = BLOCKSIZE / sizeof(int);
        int j;

        // read contents of indirect block
        int *block = malloc(BLOCKSIZE);
        if (ReadFromBlock(indir_block, block) < 0) {
            TracePrintf(0, "TraverseDirs: (ERROR) ReadFromBlock failed\n");
        }

        // iterate over every block number in the indirect block
        for (j = 0; j < ints_per_block; j++) {
            if (block[j] == 0)
                break;

            int num_dir_entries = size_remaining;
            if (size_remaining > BLOCKSIZE)
                num_dir_entries = BLOCKSIZE;
            num_dir_entries = num_dir_entries / sizeof(struct dir_entry);

            TracePrintf(0, "TraverseDirs num_dir_entries %d\n", num_dir_entries);
            short inum = TraverseDirsHelper(dir_name_copy, block[j], num_dir_entries, delete);
            if (inum > 0) {
                TracePrintf(0, "TraverseDirs Exit\n");
                return inum;
            }
            size_remaining -= BLOCKSIZE;
        }
    }
    TracePrintf(0, "TraverseDirs Exit\n");
    return -1;
}

/**
 * @brief iterates over the directory entries in "curr_block" looking for a directory
 * with the same name as "dir_name". If a match is found, return the inum in the
 * directory entry. Otherwise return -1.
 * 
 * @param dir_name the directory name we are looking for
 * @param curr_block the block in which to search
 * @param num_dir_entries the number of dir entries to iterate through
 * @param delete, should the entry with dir_name in curr_block be deleted? (If found)
 * @return short the inum at the directory entry with name "dir_name"
 */
short
TraverseDirsHelper(char *dir_name, int curr_block, int num_dir_entries, bool delete)
{
    TracePrintf(0, "TraverseDirsHelper Entry\n");
    TracePrintf(0, "TraverseDirsHelper dir_name: %s\n", dir_name);
    TracePrintf(0, "TraverseDirsHelper curr_block: %d\n", curr_block);
    TracePrintf(0, "TraverseDirsHelper num_dir_entries: %d\n", num_dir_entries);

    struct dir_entry *dir_entries = malloc(SECTORSIZE);
    if (curr_block == 0) {
        TracePrintf(0, "TraverseDirsHelper: (ERROR) curr_block is 0\n");
        // free(dir_entries);
        return -1;
    }    
        
    if (ReadFromBlock(curr_block, dir_entries) < 0) {
        TracePrintf(0, "TraverseDirsHelper: (ERROR) ReadFromBlock failed\n");
        return -1;
    }
        
    struct dir_entry *curr_entry = dir_entries;
    //int entries_per_block = BLOCKSIZE / sizeof(struct dir_entry);
    int j;
    for (j = 0; j < num_dir_entries; j++) {
        char *curr_dir_name = MakeNullTerminated(curr_entry->name, DIRNAMELEN);
        TracePrintf(0, "TraverseDirsHelper curr_dir_name: %s\n", curr_dir_name);
        TracePrintf(0, "TraverseDirsHelper dir_name: %s\n", dir_name);
        if (strcmp(curr_dir_name, dir_name) == 0) {
            short inum = curr_entry->inum;
            // free(dir_entries);
            TracePrintf(0, "TraverseDirsHelper Exit (success)\n");
            if (delete) {
                curr_entry->inum = 0;
            }
            return (inum);
        }
        curr_entry++;
    }

    // free(dir_entries);
    TracePrintf(0, "TraverseDirsHelper Exit (fail)\n");
    return -1;
}

//Put file into open file list with specific data as given by params
void
InsertOpenFile(struct open_file_list **wait, int fd, short inum)
{
    TracePrintf(0, "InsertOpenFile Entry\n");
    TracePrintf(0, "    *wait %p\n", *wait);
    TracePrintf(0, "    open_files: %p\n", open_files);
	struct open_file_list *new = malloc(sizeof(struct open_file_list));

  
	new->fd = fd;
	new->inum = inum;
    new->next = NULL;
    if (fd > 0) {
        new->blocknum = 0;
        new->position = 0;
    } else {
        new->blocknum = 0;
        new->position = 2 * sizeof(struct dir_entry);
    }
    

    struct open_file_list *list = *wait;
    TracePrintf(0, "InsertOpenFile list: %p\n", list);
    if (list == NULL) {
        *wait = new;
        // *wait = malloc(sizeof(struct open_file_list));
        // (*wait)->fd = fd;
        // (*wait)->inum = inum;
        // (*wait)->next = NULL;
        // (*wait)->blocknum = 0;
        // (*wait)->position = 0;
        
        //TracePrintf(0, "InsertOpenFile *wait: %p\n", *wait);
        //TracePrintf(0, "InsertOpenFile open_files: %p\n", open_files);
        // TracePrintf(0, "    wait->fd: %d\n", (*wait)->fd);
        // TracePrintf(0, "    *wait->inum: %d\n", (*wait)->inum);
        // TracePrintf(0, "    *wait->next: %p\n", (*wait)->next);
        // TracePrintf(0, "    *wait->blocknum: %d\n", (*wait)->blocknum);
        // TracePrintf(0, "    *wait->position: %d\n", (*wait)->position);
        // TracePrintf(0, "InsertOpenFile: Creating list from nothing\n");
        TracePrintf(0, "InsertOpenFile Exit (first one)\n");
        return;
    }

    TracePrintf(0, "InsertNode: Outside loop 1\n");
    while (list->next != NULL) {
        // TracePrintf(1, "InsertOpenFile list: %p\n", list);
        // TracePrintf(1, "list->fd: %d\n", list->fd);
        // TracePrintf(1, "list->inum: %d\n", list->inum);
        // TracePrintf(1, "InsertOpenFile list->next: %p\n", list->next);
        // TracePrintf(1, "list->blocknum: %d\n", list->blocknum);
        // TracePrintf(1, "list->position: %d\n", list->position);
        list = list->next;
    }
    TracePrintf(0, "InsertNode: Outside loop 2\n");
    list->next = new;
}

//Removes given fd from open file list
int
RemoveOpenFile(struct open_file_list **wait, int fd)
{
    TracePrintf(0, "RemoveOpenFile Entry\n");
	struct open_file_list *q = (*wait);
	struct open_file_list *prev;
    if (fd == -1) {
        TracePrintf(0, "RemoveOpenFile: Can't close directory\n");
        return -1;
    }
	if (q != NULL && q->fd == fd) {
		(*wait)->next = q->next;
		free(q);
        TracePrintf(0, "RemoveOpenFile Exit (first one)\n");
		return (0);
	}
	while (q != NULL && q->fd != fd){
		prev = q;
		q = q->next;
	}

	if (q == NULL) {
        TracePrintf(0, "RemoveOpenFile Exit (null one)\n");
		return (-1);
	}
	
	prev->next = q->next;
	free(q);
    TracePrintf(0, "RemoveOpenFile Exit (middle one)\n");
	return(0);
}

// Searches for a specific inum in open file list
struct open_file_list *
SearchOpenFile(struct open_file_list **wait, short inum)
{
    TracePrintf(0, "SearchOpenFile Entry\n");
	struct open_file_list *q = (*wait);
	if (q != NULL && q->inum == inum) {
        TracePrintf(0, "SearchOpenFile Exit (first one)\n");
		return (q);
	}
	while (q != NULL && q->inum != inum){
		q = q->next;
	}

	if (q == NULL) {
        TracePrintf(0, "SearchOpenFile Exit (null one)\n");
		return NULL;
	}
    TracePrintf(0, "SearchOpenFile Exit (middle one)\n");
	return(q);
}

struct open_file_list *
SearchByFD(struct open_file_list **wait, int fd)
{
	struct open_file_list *q = (*wait);
	if (q != NULL && q->fd == fd) {
		return (q);
	}
	while (q != NULL && q->fd != fd){
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
    //TracePrintf(0, "EditOpenFile Entry\n");
    struct open_file_list *q = (*wait);
    // TracePrintf(0, "EditOpenFile open_files: %p\n", open_files);
    // TracePrintf(0, "EditOpenFile *wait: %p\n", *wait);
    // TracePrintf(0, "EditOpenFile q: %p\n", q);

    // First file is the one we want to edit
	if (q != NULL && q->fd == fd) {
		q->blocknum = blocknum;
        q->position = position;
        // TracePrintf(0, "EditOpenFile q != NULL and q->fd == fd\n");
        // TracePrintf(0, "EditOpenFile q->blocknum: %d\n", q->blocknum);
        // TracePrintf(0, "EditOpenFile q->position: %d\n", q->position);
        //free(q);
        // TracePrintf(0, "EditOpenFile Exit\n");
		return (0);
	}

    // File we want to edit is not first
	while (q != NULL && q->fd != fd) {
		q = q->next;
        // TracePrintf(0, "EditOpenFile q != NULL and q->fd != fd\n");
        // TracePrintf(0, "EditOpenFile q->next: %p\n", q->next);
	}

	if (q == NULL) {
		return (-1);
	}

	q->blocknum = blocknum;
    q->position = position;
    // TracePrintf(0, "EditOpenFile q->blocknum: %d\n", q->blocknum);
    // TracePrintf(0, "EditOpenFile q->position: %d\n", q->position);
    //free(q);
    //TracePrintf(0, "EditOpenFile Exit\n");
	return(0);
}

//Puts fd into free fd list
void
InsertFD(struct free_fd_list **wait, int fd)
{
    TracePrintf(0, "InsertFD Entry\n");
	struct free_fd_list *new = malloc(sizeof(struct free_fd_list));
    TracePrintf(0, "InsertFD free_fds: %p\n", free_fds);
    TracePrintf(0, "InsertFD wait: %p\n", *wait);

	new->fd = fd;
    new->next = NULL;
    TracePrintf(0, "InsertFD new->fd: %d \n", new->fd);

    struct free_fd_list *list = *wait;
    if (list == NULL) {
        *wait = new;
        TracePrintf(0, "InsertFD list is null \n");
        TracePrintf(0, "InsertFD Exit \n");
        return;
    }
    TracePrintf(0, "InsertNode: Outside loop 1\n");
    while (list->next != NULL) {
        TracePrintf(0, "InsertFD list: %p\n", list);
        TracePrintf(0, "InsertFD next: %p\n", list->next);
        list = list->next;
    }
    TracePrintf(0, "InsertFD: Outside loop 2\n");
    list->next = new;
    TracePrintf(0, "InsertFD Exit\n");
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
		// free(q);
		return (min);
	}
	while (q != NULL && q->fd != min){
		prev = q;
		q = q->next;
	}

	if (q == NULL) {
		return (-2);
	}
	
	prev->next = q->next;
	// free(q);
	return(min);
}

/**
 * Create/Mkdir hybrid
 * 
 * Expected message struct: messageSinglePath
 */
void 
YFSCreateMkDir(void *m, bool dir) 
{
    TracePrintf(0, "----------YFSCreateMkDir Entry----------\n");
    struct messageSinglePath *msg = (struct messageSinglePath *) m;
    char *pathname = GetPathName(msg->pid, msg->pathname); // Free this bitch at some point

    // Get name/inum/inode for parent directory and the new file/directory
    char *null_path = MakeNullTerminated(pathname, MAXPATHNAMELEN);
    char *directory;
    char *new_file;
    int i;
    int n = strlen(null_path);
    TracePrintf(0, "YFSCreateMkDir: Dir is %d\n", dir);
    TracePrintf(0, "YFSCreateMkDir null path length: %d\n", n);
    TracePrintf(0, "YFSCreateMkDir finding last slash\n");
    TracePrintf(0, "YFSCreateMkDir pathname: %s\n", null_path);
    for (i = n - 1; i >= 0; i--) {
        if (null_path[i] == '/') {
            TracePrintf(0, "YFSCreateMkDir loop entry %d\n", i);
            new_file = malloc(n-i+1);
            int k;
            if (i > 0) {
                directory = malloc(i + 1);
                for (k = 0; k < i; k++){
                    directory[k] = null_path[k];
                }
                directory[i] = '\0';
            } else {
                directory = "";
            }
            
            for (k = 0; k < n - i + 1; k++){
                new_file[k] = null_path[k + i + 1];
            }
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
        return;
    }
    TracePrintf(0, "YFSCreateMkDir directory: %s\n", directory);
    TracePrintf(0, "YFSCreateMkDir new_file: %s\n", new_file);
    TracePrintf(0, "YFSCreateMkDir parent_inum: %d\n", parent_inum);

    struct inode *parent_node = GetInodeAt(parent_inum);
    TracePrintf(0, "YFSCreateMkDir parent type: %d\n", parent_node->type);
    if (parent_node->type == INODE_DIRECTORY) {
        struct inode *new_node;

        // Get child_inum
        short child_inum;
        if ((child_inum = TraverseDirs(new_file, parent_inum, false)) <= 0) {
            // file we're creating does not have an inode yet
            child_inum = GetFreeInode();
            if (child_inum < 0) {
                TracePrintf(0, "YFSCreateMkDir: (ERROR) child_inum <= 0\n");
                msg->retval = ERROR;
                // free(pathname);
                // free(null_path);
                // free(new_file);
                return;
            }
        } else if (dir) {
            TracePrintf(0, "YFSCreateMkDir: (ERROR) we are trying to make a directory that already exists\n");
            msg->retval = ERROR;
            // free(pathname);
            // free(null_path);
            // free(new_file);
            return;
        }
        new_node = GetInodeAt(child_inum);

        // create dir entry and add it to parent directory
        struct dir_entry *new_file_entry = CreateDirEntry(child_inum, new_file);
        int openval = InsertOFDeluxe(child_inum);
        TracePrintf(0, "YFSCreateMkDir: ADDING ENTRY TO PARENT\n");
        AddToBlock(parent_inum, new_file_entry, sizeof(struct dir_entry));
        
        // struct dir_entry *buf = (struct dir_entry *) b_cache.blocks[8];
        // for (i = 0; i < 3; i++) {
        //     TracePrintf(0, "Before WriteInum in Cmkdir block 8 position %d, entry %s\n", i, (buf+i)->name);
        // }

        // Update the inode
        MakeNewFile(new_node, dir);
        TracePrintf(0, "YFSCreateMkDir: Parent node size: %d\n", parent_node->size);
        WriteINum(child_inum, *new_node);
        
        // Add "." and ".."" entries if we are creating a directory
        TracePrintf(0, "YFSCreateMkDir: Dir is %d\n", dir);
        if (dir) {
            TracePrintf(0, "YFSCreateMkDir: creating . and ..\n");
            struct dir_entry *dot = CreateDirEntry(child_inum, ".");
            struct dir_entry *dotdot = CreateDirEntry(parent_inum, "..");
            struct dir_entry *blk = malloc(2 * sizeof(struct dir_entry)); //check in office hours, seems sus
            blk[0] = *dot;
            blk[1] = *dotdot;
            TracePrintf(0, "YFSCreateMkDir ADDING ., .. ENTRY TO CHILD\n");
            AddToBlock(child_inum, blk, 2*sizeof(struct dir_entry));
        }
        TracePrintf(0, "YFSCreateMkDir (after adding ., ..) child node->size: %d\n", new_node->size);

        if (dir) {
            msg->retval = 0;
        } else {
            msg->retval = openval;
        }
        TracePrintf(0, "YFSCreateMkDir msg->retval %d\n", msg->retval);

        //TracePrintf(0, "YFSCreateMkDir: new node type %d\n", GetInodeAt(child_inum)->type);
        //TracePrintf(0, "YFSCreateMkDir: new node size %d\n", GetInodeAt(child_inum)->size);
    } else {
        TracePrintf(0, "YFSCreateMkDir: (ERROR) parent node is not a directory\n");
        msg->retval = ERROR;
    }
    TracePrintf(0, "----------YFSCreateMkDir Exit----------\n");
    // free(pathname);
    // free(null_path);
    // free(new_file);
}

// Shoves data into empty inode struct
void
MakeNewFile(struct inode *node, bool directory) 
{
    TracePrintf(0, "MakeNewFile Entry\n");
    TracePrintf(0, "MakeNewFile: Dir is %d\n", directory);

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
    TracePrintf(0, "MakeNewFile Exit\n");
}

//Gets inum of first free inode
short
GetFreeInode() 
{
    short i;
    for (i = 0; i < ni + 1; i++) {
        if (inode_bitmap[i]) {
            inode_bitmap[i] = false;
            GetInodeAt(i)->reuse++;
            return i;
        }
    }
    TracePrintf(0, "GetFreeInode: (ERROR) no free nodes remaining.\n");
    return -1;
}

//Gets block num of first free block
int
GetFreeBlock() 
{
    int i;
    for (i = 0; i < nb + 1; i++) {
        if (block_bitmap[i]) {
            block_bitmap[i] = false;
            return i;
        }
    }
    TracePrintf(0, "GetFreeBlock: (ERROR) no free blocks remaining.\n");
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
    TracePrintf(0, "---------------ReadFromBlock------------------\n");
    TracePrintf(0, "Reading from block %d\n", sector_num);

    int gbc = GetBlockCache((short) sector_num, buf);
    if (gbc > 0) {
        TracePrintf(0, "Returning dirty bit %d\n", gbc);
        return gbc;
    } else {
        int r = ReadSector(sector_num, buf);
        TracePrintf(0, "Buf's current contents: %d\n", (char *) buf);
        if (r < 0) {
            TracePrintf(0, "ReadFromBlock readSector error\n");
            return ERROR;
        }
        TracePrintf(0, "Inserting block %d into cache\n", sector_num);
        // struct dir_entry *buf2 = (struct dir_entry *) b_cache.blocks[8];
        // int i;
        // for (i = 0; i < 3; i++) {
        //     TracePrintf(0, "In ReadFromBlock block 8 position %d, entry %s\n", i, (buf2+i)->name);
        // }
        InsertBlockCache(sector_num, buf, 0);
        // buf2 = (struct dir_entry *) b_cache.blocks[8];
        // for (i = 0; i < 3; i++) {
        //     TracePrintf(0, "In ReadFromBlock block 8 position %d, entry %s\n", i, (buf2+i)->name);
        // }
        TracePrintf(0, "ParseFileName Exit\n");
        return 0;
    }
    // int r = ReadSector(sector_num, buf);
    // TracePrintf(0, "ReadFromBlock readSector returned %d\n", r);
    // return r;
}

/**
 * Write to cache. Otherwise, read the disk and write it into the cache.
 * 
 * */
int
WriteToBlock(int sector_num, void *buf) 
{
    return InsertBlockCache(sector_num, buf, true);
}

int
WriteINum(int inum, struct inode node) 
{
    TracePrintf(0, "WriteINum Entry\n");
    TracePrintf(0, "WriteInum inum: %d\n", inum);
    TracePrintf(0, "WriteInum node->size: %d\n", node.size);
    void *block = malloc(BLOCKSIZE);
    short sector = GetSectorNum(inum);
    if (GetBlockCache(sector, block) == -1) {
        if (ReadSector(sector, block) == -1) {
            TracePrintf(0, "Couldn't read sector in WriteInum\n");
            return -1;
        }
        if (InsertBlockCache(sector, block, false) < 0) {
            TracePrintf(0, "Couldn't insert block in WriteInum\n");
            return -1;
        }
    }
    return InsertInodeCache(inum, node, true);
}

//Get an inode from an inum
struct inode *
GetInodeAt(short inum) 
{
    TracePrintf(0, "GetInodeAt Entry\n");
    struct inode *node = malloc(sizeof(struct inode));
    if (GetInodeCache(inum, node) >= 0) {
        TracePrintf(0, "GetInodeAt getting cached inode\n");
        TracePrintf(0, "GetInodeAt inum: %d\n", inum);
        TracePrintf(0, "GetInodeAt node->size: %d\n", node->size);
        TracePrintf(0, "GetInodeAt node->type: %d\n", node->type);
        return node;
    }
    struct inode *blk = malloc(BLOCKSIZE);
    int sector = GetSectorNum((int) inum);
    ReadFromBlock(sector, blk);
    struct inode *read_node = (struct inode *) blk + GetSectorPosition((int) inum);
    TracePrintf(0, "GetInodeAt inserting into inode cache\n");
    TracePrintf(0, "GetInodeAt inum: %d\n", inum);
    TracePrintf(0, "GetInodeAt node->size: %d\n", read_node->size);
    InsertInodeCache(inum, *read_node, false);
    return read_node;
}

// Add data to already open file (or directory)'s current block
// Assume always write len bytes from buf
int 
AddToBlock(short inum, void *buf, int len) 
{
    TracePrintf(0, "AddToBlock Entry\n");
    TracePrintf(0, "AddToBlock inum: %d\n", inum);
    TracePrintf(0, "AddToBlock len: %d\n", len);

    // struct dir_entry *bufff = (struct dir_entry *) b_cache.blocks[8];
    // int j;
    // for (j = 0; j < 3; j++) {
    //     TracePrintf(0, "in Addtoblock block 8 position %d, entry %s\n", j, (bufff+j)->name);
    // }

    // Get file from open file list
    struct open_file_list *file = SearchOpenFile(&open_files, inum);
    if (file == NULL) {
        TracePrintf(0, "AddToBlock: File is not open\n");
        TracePrintf(0, "AddToBlock Exit\n");
        return (-1);
    }
    int blknum = file->blocknum;
    int pos = file->position;

    // Get inode for the current file
    struct inode *node = GetInodeAt(inum);
    char *edit_blk = malloc(BLOCKSIZE);
    // If the current file has no direct blocks, get a free block
    if (node->direct[0] == 0) {
        TracePrintf(0, "AddToBlock: getting a free block for inum %d\n", inum);
        node->direct[0] = GetFreeBlock();
        TracePrintf(0, "AddToBlock: got block %d\n", node->direct[0]);
    }

    // Read contents of blknum
    if (ReadFromBlock(GetTrueBlock(node, blknum), edit_blk) < 0) {
        TracePrintf(0, "AddToBlock: Reading from block Failed\n");
        TracePrintf(0, "AddToBlock Exit\n");
        return (-1);
    }
    if (pos + len < BLOCKSIZE) {
        // Case 1: writing will not take us beyond the current block
        int i;
        int write_num = GetTrueBlock(node, blknum);
        if (write_num <= 0) {
            TracePrintf(0, "AddToBlock: Wrong Block gotten by GetTrueBlock\n");
            TracePrintf(0, "AddToBlock Exit\n");
            return (-1);
        }
        
        TracePrintf(0, "AddToBlock: writing to block %d at position %d\n", write_num, pos);
        for (i = pos; i < pos + len; i++) {
            edit_blk[i] = *((char *) (buf + i - pos));
        }

        
        TracePrintf(0, "AddToBlock: writing to block %d\n", write_num);

        WriteToBlock(write_num, edit_blk);

        // struct dir_entry *buf2 = (struct dir_entry *) b_cache.blocks[8];
        // for (j = 0; j < 3; j++) {
        //     TracePrintf(0, "in add2blk after write2blk block 8 position %d, entry %s\n", j, (buf2+j)->name);
        // }

        // update position and blocknum
        EditOpenFile(&open_files, file->fd, blknum, pos + len);

        TracePrintf(0, "AddToBlock open_files: %p \n", open_files);
        TracePrintf(0, "AddToBlock len: %d\n", len);
    } else {
        // Case 2: writing will take us beyond the current block

        int newpos;
        int i = pos;
        int write_num = GetTrueBlock(node, blknum);
        TracePrintf(0, "AddToBlock: Writing to block %d at position %d\n", write_num, pos);
        for (newpos = pos; newpos < pos + len; newpos++) {
            if (i >= BLOCKSIZE) {
                WriteToBlock(write_num, edit_blk);
                int newblk = GetFreeBlock();
                if (AddBlockToInode(node, newblk) == -1) {
                    TracePrintf(0, "AddToBlock: Couldn't add block to inode\n");
                    TracePrintf(0, "AddToBlock Exit\n");
                    return (-1);
                }

                blknum++;
                write_num = newblk;
                i -= BLOCKSIZE;
                if (i != 0) {
                    TracePrintf(0, "AddToBlock: Something wrong with newpos loop\n");
                    TracePrintf(0, "AddToBlock Exit\n");
                    return (-1);
                }
            }
            edit_blk[i] = *((char *) (buf + i - pos));
            i++;
        }
        WriteToBlock(write_num, edit_blk);
        // update position and blocknum
        EditOpenFile(&open_files, file->fd, blknum, i);
        TracePrintf(0, "AddToBlock len: %d\n", len);
    }
    // update 
    TracePrintf(0, "AddToBlock old size: %d\n", node->size);
    TracePrintf(0, "AddToBlock len: %d\n", len);
    node->size += len;
    TracePrintf(0, "AddToBlock new size: %d\n", node->size);

    WriteINum(inum, *node);

    TracePrintf(0, "AddToBlock Exit\n");
    return len;
}


int 
GetFromBlock(short inum, void *buf, int len) 
{
    TracePrintf(0, "GetFromBlock Entry\n");
    TracePrintf(0, "GetFromBlock inum: %d\n", inum);
    TracePrintf(0, "GetFromBlock len: %d\n", len);

    // Get file from open file list
    struct open_file_list *file = SearchOpenFile(&open_files, inum);
    if (file == NULL) {
        TracePrintf(0, "GetFromBlock: File is not open\n");
        TracePrintf(0, "GetFromBlock Exit\n");
        return (-1);
    }
    int blknum = file->blocknum;
    int pos = file->position;

    // Get inode for the current file
    struct inode *node = GetInodeAt(inum);
    char *read_blk = malloc(BLOCKSIZE);
    // If the current file has no direct blocks, get a free block
    if (node->direct[0] == 0 || node->size == 0) {
        TracePrintf(0, "GetFromBlock: (ERROR) Reading from a nonexistent block\n");
        return -1;
    }
    int read_num = GetTrueBlock(node, blknum);
    TracePrintf(0, "GetFromBlock: reading from block %d at position %d\n", read_num, pos);
    if (read_num <= 0) {
        TracePrintf(0, "GetFromBlock: Wrong block gotten by GetTrueBlock\n");
        TracePrintf(0, "GetFromBlock Exit\n");
        return (-1);
    }
    // Read contents of blknum
    if (ReadFromBlock(read_num, read_blk) < 0) {
        TracePrintf(0, "GetFromBlock: reading from block Failed\n");
        TracePrintf(0, "GetFromBlock Exit\n");
        return (-1);
    }
    if (pos + len < BLOCKSIZE) {
        // Case 1: reading will not take us beyond the current block
        int i;
        for (i = pos; i < pos + len; i++) {
            if (i + blknum * BLOCKSIZE == node->size) {
                break;
            }
            TracePrintf(0, "GetFromBlock character %d\n", read_blk[i]);

            * ((char *) buf + i - pos) = read_blk[i];
        }

        // update position and blocknum
        EditOpenFile(&open_files, file->fd, blknum, i);
        return i - pos;
        
    } else {
        // Case 2: reading will take us beyond the current block

        int newpos;
        int i = pos;
        for (newpos = pos; newpos < pos + len; newpos++) {
            if (newpos + blknum * BLOCKSIZE == node->size) {
                break;
            }
            if (i >= BLOCKSIZE) {
                blknum++;
                read_num = GetTrueBlock(node, blknum);
                if (ReadFromBlock(read_num, read_blk) < 0) {
                    TracePrintf(0, "GetFromBlock: Couldn't read into additional block\n");
                    return (-1);
                }
                i -= BLOCKSIZE;
                if (i != 0) {
                    TracePrintf(0, "GetFromBlock: Something wrong with newpos loop\n");
                    TracePrintf(0, "GetFromBlock Exit\n");
                    return (-1);
                }
            }
            TracePrintf(0, "GetFromBlock: reading character %s\n", read_blk[i]);
            ((char *) buf)[newpos - pos] = read_blk[i];
            i++;
        }
        // update position and blocknum
        EditOpenFile(&open_files, file->fd, blknum, i);
        TracePrintf(0, "GetFromBlock len: %d\n", len);
        TracePrintf(0, "GetFromBlock Exit\n");
        return newpos - pos;
    }
}

//Add a new block to an inode
int
AddBlockToInode(struct inode *node, int block_num) 
{
    TracePrintf(0, "AddBlockToInode Entry\n");
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
        TracePrintf(0, "AddBlockToInode: somehow you've run out of space in the indirect block!\n");
        TracePrintf(0, "AddBlockToInode Exit\n");
        return (-1);
    }
    WriteToBlock(node->indirect, ind_block);
    TracePrintf(0, "AddBlockToInode Exit\n");
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
            TracePrintf(0, "GetTrueBlock: (ERROR) ReadFromBlock failed\n");
            return -1;
        }
        return ind_blk[blocknum - NUM_DIRECT];
    }
}

//Insert an inum into the open file list and conduct extra checks
int
InsertOFDeluxe(short inum) 
{
    TracePrintf(0, "InsertOFDeluxe Entry\n");
    TracePrintf(0, "InsertOFDeluxe inum: %d\n", inum);

    int new_fd;
    if (GetInodeAt(inum)->type == INODE_DIRECTORY) {
        TracePrintf(0, "InsertOFDeluxe: we have a directory\n");
        new_fd = -1;
    } else {
        TracePrintf(0, "InsertOFDeluxe: we have a file\n");
        new_fd = RemoveMinFD(&free_fds);
        TracePrintf(0, "New fd: %d\n", new_fd);
        if (new_fd == -2) {
            TracePrintf(0, "InsertOFDeluxe: there is no reused fd\n");
            new_fd = cur_fd;
            cur_fd++;
        }
    }
    if (new_fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    
    InsertOpenFile(&open_files, new_fd, inum);
    TracePrintf(0, "InsertOFDeluxe Exit\n");
    return new_fd;
}

int
CeilDiv(int a, int b) {
    if (a % b == 0)
        return a / b;
    else 
        return a / b + 1;
} 

/**
 * @brief Checks that "node" is a directory with no entries except
 * "." and ".."
 *
 * @param node 
 * @return true 
 * @return false 
 */
bool
CheckDir(struct inode *node)
{
    TracePrintf(0, "CheckDir Entry\n");
    if (node->type != INODE_DIRECTORY) {
        TracePrintf(0, "CheckDir: (ERROR) input node is not a directory\n");
        return false;
    }
    int size_remaining = node->size;
    int i;
    for (i = 0; i < NUM_DIRECT; i++) {
        TracePrintf(0, "CheckDir traversing direct block %d\n", i);
        int curr_block = node->direct[i];

        if (curr_block == 0)
            break;

        int num_dir_entries = size_remaining;
        if (size_remaining > BLOCKSIZE)
            num_dir_entries = BLOCKSIZE;
        num_dir_entries = num_dir_entries / sizeof(struct dir_entry);

        if (!CheckDirHelper(curr_block, num_dir_entries)) {
            TracePrintf(0, "CheckDir Exit (false)\n");
            return false;
        }
        size_remaining -= BLOCKSIZE;
    }

    // iterate over indirect block
    int indir_block = node->indirect;
    if (indir_block != 0) {
        int ints_per_block = BLOCKSIZE / sizeof(int);
        int j;

        // read contents of indirect block
        int *block = malloc(BLOCKSIZE);
        if (ReadFromBlock(indir_block, block) < 0) {
            TracePrintf(0, "CheckDir: (ERROR) ReadFromBlock failed\n");
        }

        // iterate over every block number in the indirect block
        for (j = 0; j < ints_per_block; j++) {
            if (block[j] == 0)
                break;

            int num_dir_entries = size_remaining;
            if (size_remaining > BLOCKSIZE)
                num_dir_entries = BLOCKSIZE;
            num_dir_entries = num_dir_entries / sizeof(struct dir_entry);

            TracePrintf(0, "CheckDir num_dir_entries %d\n", num_dir_entries);
            if (!CheckDirHelper(block[j], num_dir_entries)) {
                TracePrintf(0, "CheckDir Exit (false)\n");
                return false;
            }
            size_remaining -= BLOCKSIZE;
        }
    }
    TracePrintf(0, "CheckDir Exit (true)\n");
    return true;
}

/**
 * @brief Helper function to CheckDir. Looks at "num_dir_entries" directory entries in
 * "curr_block" and checks that they are all invalid (except "." and "..")
 * 
 * @param curr_block the block with directory entries
 * @param num_dir_entries the number of directory entries to look at
 * @return true 
 * @return false 
 */
bool
CheckDirHelper(int curr_block, int num_dir_entries)
{
    TracePrintf(0, "CheckDirHelper Entry\n");
    TracePrintf(0, "CheckDirHelper curr_block: %d\n", curr_block);
    TracePrintf(0, "CheckDirHelper num_dir_entries: %d\n", num_dir_entries);
    struct dir_entry *dir_entries = malloc(SECTORSIZE);
    if (curr_block == 0) {
        // free(dir_entries);
        return -1;
    }    
        
    if (ReadFromBlock(curr_block, dir_entries) < 0) {
        TracePrintf(0, "CheckDirHelper: (ERROR) ReadFromBlock failed\n");
        Exit(-1);
    }
        
    struct dir_entry *curr_entry = dir_entries;
    int j;
    for (j = 0; j < num_dir_entries; j++) {
        TracePrintf(0, "CheckDirHelper curr_entry->name: %s\n", MakeNullTerminated(curr_entry->name, DIRNAMELEN));
        TracePrintf(0, "CheckDirHelper curr_entry->inum: %d\n", curr_entry->inum);
        if (strcmp(MakeNullTerminated(curr_entry->name, DIRNAMELEN), ".") == 0) {
            curr_entry++;
            continue;
        } else if (strcmp(MakeNullTerminated(curr_entry->name, DIRNAMELEN), "..") == 0) {
            curr_entry++;
            continue;
        }
        if (curr_entry->inum != 0) {
            // free(dir_entries);
            TracePrintf(0, "CheckDirHelper Exit (false)\n");
            return (false);
        }
        curr_entry++;
    }

    // free(dir_entries);
    TracePrintf(0, "CheckDirHelper Exit (true)\n");
    return true;
}

void
FreeFileBlocks(struct inode *node)
{
    TracePrintf(0, "FreeFileBlocks Entry\n");
    int i;
    for (i = 0; i < NUM_DIRECT; i++) {
        TracePrintf(0, "FreeFileBlocks traversing direct block %d\n", i);
        int curr_block = GetTrueBlock(node, i);

        if (curr_block == 0)
            break;

        block_bitmap[curr_block] = 1;
    }

    // iterate over indirect block
    int indir_block = node->indirect;
    if (indir_block != 0) {
        int ints_per_block = BLOCKSIZE / sizeof(int);
        int j;

        // read contents of indirect block
        int *block = malloc(BLOCKSIZE);
        if (ReadFromBlock(indir_block, block) < 0) {
            TracePrintf(0, "FreeFileBlocks: (ERROR) ReadFromBlock failed\n");
            return;
        }

        // iterate over every block number in the indirect block
        for (j = 0; j < ints_per_block; j++) {
            if (block[j] == 0)
                break;

            block_bitmap[block[j]] = 1;
        }
    }
}

int
InsertBlockCache(short blknum, void *block, bool dirty)
{
    short oghash = blknum % BLOCK_CACHESIZE;
    

    short hash;
    bool found = false;
    TracePrintf(0, "inserting %d into cache\n", (char *) block);
    TracePrintf(0, "Block cache OG hash is %d\n", oghash);
    TracePrintf(0, "Block cache counter is %d\n", b_cache.counter);
    int i;
    for (i = 0; i < BLOCK_CACHESIZE; i++) {
        hash = (oghash + i) % BLOCK_CACHESIZE;
        if (b_cache.nums[hash] == 0 || b_cache.nums[hash] == blknum) {
            TracePrintf(0, "Found Block in cache\n");
            found = true;
            break;
        }
    }

    if (!found) {
        //write back algo
        //write back algo
        TracePrintf(0, "LRU Replacement\n");

        int min = INT_MAX;
        int i;
        for (i = 0; i < BLOCK_CACHESIZE; i++) {
            int test = i_cache.lru[hash];
            if (test < min) {
                min = test;
                hash = i;
            }
        }
        TracePrintf(0, "min lru is %d\n", min);

    }
    
    TracePrintf(0, "Inserting at position %d\n", hash);

    if (b_cache.dirty_bits[hash]) {
        WriteSector(b_cache.nums[hash], &(b_cache.blocks[hash]));
    }

    // copy from block into cache
    b_cache.nums[hash] = blknum;

    struct dir_entry *buf2 = (struct dir_entry *) b_cache.blocks[8];
    for (i = 0; i < 3; i++) {
        TracePrintf(0, "In insert pre-memcpy block 8 position %d, entry %s\n", i, (buf2+i)->name);
    }
    TracePrintf(0, "initial block contents: %s\n", (char *) block);
    TracePrintf(0, "initial block hash contents: %s\n", b_cache.blocks[hash]);
    memcpy(b_cache.blocks[hash], block, BLOCKSIZE);
    // char *curr_cache_entry = (char *) &(b_cache.blocks[hash]);
    // char *curr_block_entry = (char *) block;
    // for (i = 0; i < BLOCKSIZE; i++) {
    //     curr_cache_entry[i] = curr_block_entry[i];
    // }
    buf2 = (struct dir_entry *) b_cache.blocks[8];
    for (i = 0; i < 3; i++) {
        TracePrintf(0, "In insert post-memcpy block 8 position %d, entry %s\n", i, (buf2+i)->name);
    }
    TracePrintf(0, "b_cache.blocks[hash] contents: %s\n", b_cache.blocks[hash]);

    b_cache.lru[hash] = b_cache.counter;
    b_cache.dirty_bits[hash] = dirty;
    b_cache.counter++;
   
    TracePrintf(0, "InsertBlockCache counter is %d\n", b_cache.counter);

    TracePrintf(0, "InsertBlockoCache Exit\n");
    return 0;
}

int
InsertInodeCache(short inum, struct inode node, bool dirty)
{
    TracePrintf(0, "InsertInodeCache Entry\n");
    TracePrintf(0, "InsertInodeCache inum: %d\n", inum);

    short oghash = inum % INODE_CACHESIZE;
    short hash;
    bool found = false;
    TracePrintf(0, "Inserting node of size %d\n", node.size);
    TracePrintf(0, "Inode OG hash is %d\n", oghash);
    TracePrintf(0, "Inode cache counter is %d\n", i_cache.counter);
    int i;
    for (i = 0; i < INODE_CACHESIZE; i++) {
        hash = (oghash + i) % INODE_CACHESIZE;
        if (i_cache.nums[hash] == 0 || i_cache.nums[hash] == inum) {
            found = true;
            TracePrintf(0, "InsertInodeCache found Inode in cache\n");
            break;
        }
    }
    if (!found) {
        //write back algo
        int min = INT_MAX;
        int i;
        for (i = 0; i < INODE_CACHESIZE; i++) {
            int test = i_cache.lru[hash];
            if (test < min) {
                min = test;
                hash = i;
            }
        }
        TracePrintf(0, "InsertInodeCache min lru is %d\n", min);
    }
    
    TracePrintf(0, "InsertInodeCache inserting at position %d\n", hash);
    if (i_cache.dirty_bits[hash]) {
        TracePrintf(0, "InsertInodeCache Dirty is true\n", hash);
        int removenum = i_cache.nums[hash];
        int sector = GetSectorNum(removenum);
        int position = GetSectorPosition(removenum);
        TracePrintf(0, "InsertInodeCache writing inum %d to block %d at position %d\n", removenum, sector, position);
        struct inode *nodes = malloc(BLOCKSIZE);
        if (ReadFromBlock(sector, nodes) < 0) {
            TracePrintf(0, "InsertInodeCache Writing inum can't read block\n");
            return -1;
        }
        nodes[position] = node;
        i_cache.nums[hash] = inum;
        i_cache.dirty_bits[hash] = dirty;
        i_cache.nodes[hash] = node;
        i_cache.lru[hash] = i_cache.counter;
        i_cache.counter++;
        return WriteSector(sector, nodes);
    }
    TracePrintf(0, "InsertInodeCache node hash %d\n", hash);
    i_cache.nums[hash] = inum;
    i_cache.dirty_bits[hash] = dirty;
    i_cache.nodes[hash] = node;
    i_cache.lru[hash] = i_cache.counter;
    i_cache.counter++;
    TracePrintf(0, "GetInodeCache Nodes 2 size: %d\n", i_cache.nodes[2].size);
    TracePrintf(0, "InsertInodeCache counter is %d\n", i_cache.counter);
//  
    TracePrintf(0, "InsertInodeCache Exit\n");
    return 0;
}

int
GetBlockCache(short index, void *buf)
{
    int hash = index % BLOCK_CACHESIZE;
    int j = 0;
    while (b_cache.nums[hash] != index) {
        if (b_cache.nums[hash] == 0) {
            return -1;
        }
        hash++;
        hash %= BLOCK_CACHESIZE;
        if (j > BLOCK_CACHESIZE) {
            return -1;
        }
        j++;
    }
    TracePrintf(0, "Buf now contains %s\n", (char *) buf);
    // copy data from block cache into buf
    //int i;
    //char *char_block = (char *) &(b_cache.blocks[hash]);
    TracePrintf(0, "Shoving in %s\n", (char *) b_cache.blocks[hash]);
    TracePrintf(0, "Shoving into %p\n", (char *) b_cache.blocks[hash]);
    TracePrintf(0, "ptr 7 is %p\n", (char *) b_cache.blocks[7]);
    memcpy(buf, b_cache.blocks[hash], BLOCKSIZE);
    // char *char_buf = (char *) buf;
    // for (i = 0; i < BLOCKSIZE; i++) {
    //     //TracePrintf(0, "char_block[i]: %s\n", char_block[i]);
    //     char_buf[i] = char_block[i];
    // }
    TracePrintf(0, "Buf now contains %s\n", (char *) buf);

    b_cache.lru[hash] = b_cache.counter;
    b_cache.counter++;
    TracePrintf(0, "Filled buf at hash index %d\n", hash);
    return b_cache.dirty_bits[hash];

}

int
GetInodeCache(short index,  struct inode *buf)
{
    int hash = index % INODE_CACHESIZE;
    int j = 0;
    while (i_cache.nums[hash] != index) {
        if (i_cache.nums[hash] == 0) {
            return -1;
        }
        hash++;
        hash %= INODE_CACHESIZE;
        if (j > INODE_CACHESIZE) {
            return -1;
        }
        j++;
    }

    TracePrintf(0, "GetInodeCache Nodes 2 size: %d\n", i_cache.nodes[2].size);


    // copy data from block cache into buf
    *buf = i_cache.nodes[hash];
    i_cache.lru[hash] = b_cache.counter;
    i_cache.counter++;
    return i_cache.dirty_bits[hash];
}