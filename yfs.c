#include <comp421/filesystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <comp421/iolib.h>
#include <comp421/hardware.h>

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
}

struct free_fd_list {
    int fd;
    struct free_fd_list *next;
}

struct message {
    int type;
    pid_t pid;
    int retval;
    char text[20];
};

bool *block_bitmap;
bool *inode_bitmap;

int cur_fd;
int nb;
int ni;

struct message *message;
struct fs_header header;
struct open_file_list *open_files;
struct free_fd_list *free_fds;

void YFSOpen(struct message *msg);
void YFSClose(struct message *msg);
void YFSCreate(struct message *msg);
void YFSRead(struct message *msg);
void YFSWrite(struct message *msg);
void YFSSeek(struct message *msg);
void YFSLink(struct message *msg);
void YFSUnlink(struct message *msg);
void YFSMkDir(struct message *msg);
void YFSRmDir(struct message *msg);
void YFSChDir(struct message *msg);
void YFSStat(struct message *msg);
void YFSSync(struct message *msg);
void YFSShutdown(struct message *msg);

void YFSCreateMkDir(struct message *msg, bool directory);

char *GetPathName(struct message *msg, int text_pos);
char *GetBufContents(struct message *msg, int text_pos, int len);
char *MakeNullTerminated(char *, int max_len);
short TraverseDirs(char *dir_name, short dir_inum);
short TraverseDirsHelper(char *dir_name, int curr_block);
void InsertOpenFile(struct open_file_list **wait, int fd, short inum);
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
struct dir_entry CreateDirEntry(short inum, char *name);

int GetSectorNum(int inum);
int GetSectorPosition(int inum);
struct inode *GetInodeAt(short inum);

int AddToBlock(short inum, void *buf, int len);
int AddBlockToInode(struct inode *node, int block_num);




int 
main(char *argc, char **argv) 
{
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
void
YFSOpen(struct message *msg)
{
    char *pathname = GetPathName(msg, 0); // Free this bitch at some point
    short inum = ParseFileName(pathname);
    struct inode *my_node = GetInodeAt(inum);
    if (my_node->type != INODE_REGULAR) {
        TracePrintf(0, "Tried to use Open on a directory\n");
        msg->retval = ERROR;
        return;
    }
    int new_fd = RemoveMinFD(&free_fds);
    if (new_fd == -1) {
        new_fd = cur_fd;
    }
    if (inum <= 0) {
        TracePrintf(0, "ParseFilename found bullshit\n");
        msg->retval = ERROR;
        return;
    }
    else {
        msg->retval = new_fd;
        if (new_fd == cur_fd) {
            cur_fd++;
        }
        InsertOpenFile(&open_files, cur_fd, inum);
    }
}

/**
 * Close
 * 
 * Expected message struct
 *     text[0:4] fd (file descriptor number)
 */
void
YFSClose(struct message *msg)
{
    int fd = (int) message->text[0]; // check if cast is correct
    InsertFD(&free_fds, fd);
    if (RemoveOpenFile(&open_files, fd) == -1) {
        msg->retval = ERROR;
    } 
    else {
        meg->retval = 0;
    }
}

/**
 * Create 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 */
void
YFSCreate(struct message *msg)
{
    YFSCreateMkDir(msg, false);
}

/**
 * Read 
 * 
 * Expected message struct 
 *     text[0:4] int fd (file descriptor)
 *     text[4:12] ptr to buf
 *     text[12:16] int size
 */
void
YFSRead(struct message *msg)
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
void
YFSWrite(struct message*msg)
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
void
YFSSeek(struct message *msg)
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
void
YFSLink(struct message *msg)
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
void 
YFSUnlink(struct message *msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
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
 * Expected message struct 
 *     text[0:8] ptr to pathname
 */
void
YFSMkDir(struct message *msg)
{
    YFSCreateMkDir(msg, true);
}

/**
 * RmDir 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 */
void
YFSRmDir(struct message *msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

/**
 * ChDir 
 * 
 * Expected message struct 
 *     text[0:8] ptr to pathname
 */
void
YFSChDir(struct message *msg)
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
void
YFSStat(struct message *msg)
{
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
}

/**
 * Sync 
 * 
 * Expected message struct
 *     text has no meaningful contents 
 */
void
YFSSync(struct message *msg)
{

}

/**
 * Shutdoen 
 * 
 * Expected message struct 
 *     text has no meaningful contents 
 */
void
YFSShutdown(struct message *msg)
{
    Exit(-1);
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

short
ParseFileName(char *filename) 
{  
    char *new_filename = MakeNullTerminated(filename, MAXPATHNAMELEN);
    
    char *token = strtok(new_filename, '/');
    short inum = (token, 1);
    while (token != 0) {
        token = strtok(0, '/');
        inum = TraverseDirs(token, inum);
    }
    free(new_filename);
    return inum;
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
    if (ReadFromBlock(sector_num, first_inodes) != 0) {
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
        
    if (ReadFromBlock(curr_block, dir_entries) != 0) {
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

int
RemoveOpenFile(struct open_file_list **wait, int fd)
{
	struct open_file_list *q = (*wait);
	struct open_file_list *prev;
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

struct open_file_list *
SearchOpenFile(struct open_file_list **wait, short inum)
{
	struct open_file_list *q = (*wait);
	struct open_file_list *prev;
	if (q != NULL && q->inum == inum) {
		return (q);
	}
	while (q != NULL && q->inum != inum){
		prev = q;
		q = q->next;
	}

	if (q == NULL) {
		return NULL;
	}
	return(q);
}

int
EditOpenFile(struct open_file_list **wait, int fd, int blocknum, int position) {
    struct open_file_list *q = (*wait);
	struct open_file_list *prev;
	if (q != NULL && q->fd == fd) {
		q->blocknum = blocknum;
        q->position = position;
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

	q->blocknum = blocknum;
    q->position = position;
    free(q);
	return(0);
}

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
		return (-1);
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
YFSCreateMkDir(struct message *msg, bool dir) 
{
    struct inode *block;
    char *pathname = GetPathName(message, 0); // Free this bitch at some point
    char *null_path = MakeNullTerminated(pathname, MAXPATHNAMELEN);
    char *directory;
    char *new_file;
    int i;
    int n = strlen(null_path);
    for (i = n - 1; i >= 0; i++) {
        if (null_path[i] == '/') {
            directory = malloc(i-1);
            new_file = malloc(n-i+1);
            *directory = *null_path;
            *new_file = *(null_path + i + 1);
        }
    }
    if (i == 0) {
        directory = "";
        new_file = null_path;
    }
    short parent_inum = ParseFileName(directory);
    if (inum <= 0) {
        msg->retval = ERROR;
    }
    block = malloc(BLOCKSIZE);
    if (ReadFromBlock((int) (parent_inum / IPB + 1), block) == -1) {
        TracePrintf(0, "Freak the Fuck out\n");
        Exit(-1);
    }
    int sector_num = ((int) parent_inum / IPB) + 1;
    int diff = (int) parent_inum - ((sector_num - 1) * IPB);
    struct inode *cur_node = block + diff;
    if (cur_node->type == INODE_DIRECTORY) {
        //make new file
        struct inode *new_node;
        short child_inum;
        if ((child_inum = TraverseDirs(new_file, parent_inum)) <= 0) {
            child_inum = GetFreeInode();
            if (child_inum < 0) {
                msg->retval = ERROR;
                free(pathname);
                return;
            }
        }
        if (dir) {
            msg->retval = ERROR;
            free(pathname);
            return;
        } 
        else {
            new_node = GetInodeAt(child_inum);
        }
        struct dir_entry new_file_entry = CreateDirEntry(child_inum, new_file);
        AddToBlock(parent_inum, &new_file_entry, sizeof(struct dir_entry));
        MakeNewFile(new_node, dir);
        if (dir) {
            struct dir_entry dot = CreateDirEntry(child_inum, ".");
            struct dir_entry dotdot = CreateDirEntry(parent_inum, "..");
            struct dir_entry *blk = malloc(2 * sizeof(struct dir_entry)); //check in office hours, seems sus
            int blknum = GetFreeBlock();
            blk[0] = dot;
            blk[1] = dotdot;
            AddToBlock(child_inum, blk, 2*sizeof(struct dir_entry));
        }
        WriteINum(child_inum, *new_node);
        free(pathname);
        if (dir) {
            msg->retval = 0;
        }
        else {
            YFSOpen(msg);
        }
    }
    else {
        TracePrintf(0, "Not Directory Freak the Fuck Out\n");
        free(pathname);
        msg->retval = ERROR;
    }
}


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

int 
GetSectorNum(int inum) 
{
    return (inum / IPB) + 1;
}

int
GetSectorPosition(int inum) 
{
    int sector_num = GetSectorNum(inum);
    return (inum - ((sector_num - 1) * IPB));
}

struct dir_entry 
CreateDirEntry(short inum, char *name) 
{
    struct dir_entry entry;
    entry.inum = inum;
    entry.name = name;
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

struct inode *
GetInodeAt(short inum) 
{
    struct inode *blk = malloc(BLOCKSIZE);
    int sector = GetSectorNum((int) inum);
    ReadFromBlock(sector, blk);
    return sector + GetSectorPosition((int) inum);
}

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
        WriteINum(inum, node);
    }
    return 0;
}

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
    for (j = 0; j < (BLOCKSIZE / sizeof(int)); j++) {
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