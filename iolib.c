#include <comp421/filesystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
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
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 0;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = cd + pathname;
    }
    Send(new_message, new_message->pid);
    return 0;
}

int
Close(int fd) {
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 1;
    new_message->pid = GetPid();
    new_message->text[0] = fd;
    Send(new_message, new_message->pid);
    return 0;
}

int
Create(char *pathname)
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 2;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = cd + pathname;
    }
    Send(new_message, new_message->pid);
    return 0;
}

int
Read(int fd, void *buf, int size)
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 3;
    new_message->pid = GetPid();
    new_message->text[0] = fd;
    new_message->text[4] = buf;
    new_message->text[12] = size; 
    Send(new_message, new_message->pid);
    return 0;
}

int
Write(int fd, void *buf, int size)
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 4;
    new_message->pid = GetPid();
    new_message->text[0] = fd;
    new_message->text[4] = buf;
    new_message->text[12] = size; 
    Send(new_message, new_message->pid);
    return 0;
}

int
Seek(int fd, int offset, int whence)
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 5;
    new_message->pid = GetPid();
    new_message->text[0] = fd;
    new_message->text[4] = offset;
    new_message->text[8] = whence; 
    Send(new_message, new_message->pid);
    return 0;
}

int 
Link(char *oldname, char *newname) 
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 6;
    new_message->pid = GetPid();
    
    if (oldname[0] == '/') {
        // absolute filename
        new_message->text[0] = oldname;
    } else {
        // relative filename
        new_message->text[0] = cd + oldname;
    }

    if (newname[0] == '/') {
        // absolute filename
        new_message->text[8] = newname;
    } else {
        // relative filename
        new_message->text[8] = cd + newname;
    }
    Send(new_message, new_message->pid);
}

int 
Unlink(char *pathname) 
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 7;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = cd + pathname;
    }
    Send(new_message, new_message->pid);
}

int 
SymLink(char *oldname, char *newname) 
{
    return 0;
}

int 
ReadLink(char *pathname, char *buf, int len) 
{
    /* struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 9;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = cd + pathname;
    }
    new_message->text[8] = buf;
    new_message->text[16] = len; 
    Send(new_message, new_message->pid); */
    return 0;
}

int 
MkDir(char *pathname) 
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 10;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = cd + pathname;
    }
    Send(new_message, new_message->pid);
    return 0;
}

int 
RmDir(char *pathname) 
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 11;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = cd + pathname;
    }
    Send(new_message, new_message->pid);
    return 0;
}

int 
ChDir(char *pathname) 
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 12;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = cd + pathname;
    }
    Send(new_message, new_message->pid);
    return 0;
}

int 
Stat(char *pathname, struct Stat *statbuf) 
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 13;
    new_message->pid = GetPid();
    if (pathname[0] == '/') {
        // absolute filename
        new_message->text[0] = pathname;
    } else {
        // relative filename
        new_message->text[0] = cd + pathname;
    }
    new_message->text[8] = statbuf;
    Send(new_message, new_message->pid);
    return 0;
}

int 
Sync(void) 
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 14;
    new_message->pid = GetPid();
    Send(new_message, new_message->pid);
    return 0;
}

int 
Shutdown(void) 
{
    struct message *new_message = malloc(sizeof(struct message));
    new_message->type = 15;
    new_message->pid = GetPid();
    Send(new_message, new_message->pid);
    return 0;
}

