#include <comp421/filesystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <comp421/iolib.h>
#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include "message.h"


int Link(char *oldname, char *newname);
int Unlink(char *pathname);
int SymLink(char *oldname, char *newname);
int ReadLink(char *pathname, char *buf, int len);
int MkDir(char *pathname);
int RmDir(char *pathname);
int ChDir(char *pathname);
int Stat(char *pathname, struct Stat *statbuf);
int Sync(void);
int Shutdown(void);

// void InsertNode(struct node_list **wait, struct inode *node, struct file_tree *parent, char *name);
// int RemoveNode(struct node_list **wait, struct inode node);
// struct file_tree *ParsePathName(char *pathname);

struct inode open_files[MAX_OPEN_FILES];
short cd;
// struct file_tree *root;
// struct file_tree *cd;

// struct node_list {
//     struct file_tree *first;
//     struct node_list *rest;
// }

// struct file_tree {
//     struct inode *cur;
//     struct file_tree *parent;
//     struct node_list *children;
//     char *name;
// }

int
Open(char *pathname) {
    TracePrintf(0, "Performing an actual open call\n");
    struct messageSinglePath *new_message = malloc(sizeof(struct messageSinglePath));
    new_message->type = 0;
    new_message->pid = GetPid();
    new_message->pathname = pathname;
    if (pathname[0] == '/') {
        // absolute filename
        new_message->cd = 1;
    } else {
        new_message->cd = cd;
    }
    // *message = *new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int
Close(int fd) {
    struct messageFDSizeBuf *new_message = malloc(sizeof(struct messageFDSizeBuf));
    new_message->type = 1;
    new_message->pid = GetPid();
    new_message->fd = fd;
    // *message = * (struct messageSinglePath *) new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int
Create(char *pathname)
{
    TracePrintf(0, "-------------------------CREATE-----------------------------\n");
    struct messageSinglePath *new_message = malloc(sizeof(struct messageSinglePath));
    new_message->type = 2;
    new_message->pid = GetPid();
    new_message->pathname = pathname;
    if (pathname[0] == '/') {
        // absolute filename
        new_message->cd = 1;
    } else {
        new_message->cd = cd;
    }
    // *message = *new_message;
    TracePrintf(0, "Filled message\n");
    Send(new_message, -FILE_SERVER);
    TracePrintf(0, "Message Returned: %d\n", new_message->retval);
    return new_message->retval;
}

int
Read(int fd, void *buf, int size)
{
    struct messageFDSizeBuf *new_message = malloc(sizeof(struct messageFDSizeBuf));
    new_message->type = 3;
    new_message->pid = GetPid();
    new_message->fd = fd;
    new_message->buf = buf;
    new_message->size = size; 
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int
Write(int fd, void *buf, int size)
{
    struct messageFDSizeBuf *new_message = malloc(sizeof(struct messageFDSizeBuf));
    new_message->type = 4;
    new_message->pid = GetPid();
    new_message->fd = fd;
    new_message->buf = buf;
    new_message->size = size;
    // *message = * (struct messageSinglePath *) new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int
Seek(int fd, int offset, int whence)
{
    struct messageSeek *new_message = malloc(sizeof(struct messageSeek));
    new_message->type = 5;
    new_message->pid = GetPid();
    new_message->fd = fd;
    new_message->offset = offset;
    new_message->whence = whence; 
    // *message = * (struct messageSinglePath *) new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int 
Link(char *oldname, char *newname) 
{
    struct messageDoublePath *new_message = malloc(sizeof(struct messageDoublePath));
    new_message->type = 6;
    new_message->pid = GetPid();
    new_message->pathname1 = oldname;

    if (oldname[0] == '/') {
        // absolute filename
        new_message->cd1 = 0;
    } else {
        // relative filename
        new_message->cd1 = cd;
    }
    new_message->pathname2 = newname;
    if (newname[0] == '/') {
        // absolute filename
        new_message->cd2 = 0;

    } else {
        // relative filename
        new_message->cd2 = cd;
    }
    // *message = * (struct messageSinglePath *) new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int 
Unlink(char *pathname) 
{
    struct messageSinglePath *new_message = malloc(sizeof(struct messageSinglePath));
    new_message->type = 7;
    new_message->pid = GetPid();
    new_message->pathname = pathname;
    if (pathname[0] == '/') {
        // absolute filename
        new_message->cd = 1;
    } else {
        new_message->cd = cd;
    }
    // *message = *new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int 
SymLink(char *oldname, char *newname) 
{
    (void) oldname;
    (void) newname;
    return 0;
}

int 
ReadLink(char *pathname, char *buf, int len) 
{
    (void) pathname;
    (void) buf;
    (void) len;
    /* struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 9;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = strcat(cd, MakeNullTerminated(pathname, MAXPATHNAMELEN));;
    }
    new_message->text[8] = buf;
    new_message->text[16] = len; 
    Send(new_message, new_message->pid); */
    return 0;
}

int 
MkDir(char *pathname) 
{
    struct messageSinglePath *new_message = malloc(sizeof(struct messageSinglePath));
    new_message->type = 10;
    new_message->pid = GetPid();
    new_message->pathname = pathname;
    if (pathname[0] == '/') {
        // absolute filename
        new_message->cd = 1;
    } else {
        new_message->cd = cd;
    }
    // *message = *new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int 
RmDir(char *pathname) 
{
    struct messageSinglePath *new_message = malloc(sizeof(struct messageSinglePath));
    new_message->type = 11;
    new_message->pid = GetPid();
    new_message->pathname = pathname;
    if (pathname[0] == '/') {
        // absolute filename
        new_message->cd = 1;
    } else {
        new_message->cd = cd;
    }
    // *message = *new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int 
ChDir(char *pathname) 
{
    struct messageSinglePath *new_message = malloc(sizeof(struct messageSinglePath));
    new_message->type = 12;
    new_message->pid = GetPid();
    new_message->pathname = pathname;
    if (pathname[0] == '/') {
        // absolute filename
        new_message->cd = 1;
    } else {
        // relative filename
        new_message->cd = cd;
    }
    // *message = *new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int 
Stat(char *pathname, struct Stat *statbuf) 
{
    struct messageDoublePath *new_message = malloc(sizeof(struct messageDoublePath));
    new_message->type = 13;
    new_message->pid = GetPid();
    new_message->pathname1 = pathname;
    if (pathname[0] == '/') {
        // absolute filename
        new_message->cd1 = 1;
    } else {
        new_message->cd1 = cd;
    }
    new_message->pathname2 = statbuf;
    // *message = * (struct messageSinglePath *) new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int 
Sync(void) 
{
    struct messageSinglePath *new_message = malloc(sizeof(struct messageSinglePath));
    new_message->type = 14;
    new_message->pid = GetPid();
    // *message = *new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

int 
Shutdown(void) 
{
    struct messageSinglePath *new_message = malloc(sizeof(struct messageSinglePath));
    new_message->type = 15;
    new_message->pid = GetPid();
    // *message = *new_message;
    Send(new_message, -FILE_SERVER);
    return new_message->retval;
}

