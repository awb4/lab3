# COMP 421 Lab 3 - File System

## Team Info
- Alex Berlaga - awb4
- Eduardo Landin - eal9

## Implementation Details

### `message.h`
Message types:
- `struct messageSinglePath` used for Open, Create, Unlink, MkDir, RmDir, ChDir, Sync, and Shutdown calls.
- `struct messageDoublePath` used for Link and Stat
- `struct messageFDSizeBuf` used for Close, Read, and Write
- `struct messageSeek` used for Seek
- 
These messages share the following fields:
- `int type` the type of the message
- `pid_t pid` the pid of the user process that sends the message
- `int retval` the yfs server will put the return value of the `YFS` call in this field

### `iolib.a`
Each of the methods in `iolib.a` creates a message based on the specifications above and the parameters passed to the method. The methods block the running process and wait for a reply from `yfs`.

`yfs` replies with the same message that it receives but it overwrites the `retval` field in the message with the return value for the `iolib.a` method that sent the message.

### `yfs`

Data structures:
- `struct open_file_list` a struct that represents a node in the linked list of open files. Keeps track of the file's `fd`, its current position, its `inum` and the value of its inode's `reuse` field when the file was first open.
- `struct block_cache` a struct that represents the block cache. Keeps track of the last time each element in the cache was referenced (`lru`), each element's block number (`nums`), whether each element is diry or not (`dirty_bits`), and each element's contents (`blocks`).
- `struct inode_cache` a struct that represents the inode cache. Keeps track of the last time each element in the cache was referenced (`lru`), each element's inum(`nums`), and whether each element is diry or not (`dirty_bits`).
- `block_bitmap` a bitmap that keeps track of free blocks.
- `inode_bitmap` a bitmap that keeps track of free inodes.

Main methods:
- `main` initializes caches and bitmaps. Enters an infinite for loop where it repeatedly reads a message, delegates to the appropriate YFS message processing method and replies to the user process.
- YFS message processing methods. There is one method for each of the YFS methods in `iolib.a`.
- `YFSCreateMkDir` a function that takes care of both `Create` and `MkDir` calls.

Helper methods: there's a lot of these
  - `ParseFileName` takes in a pathname to a file and returns corresponding inum
    - `TraverseDirs` and `TraverseDirsHelper` help this function by going through the directory entries in the parent directory and searching for the child file.
  - `GetPathName` and `GetBufContents` are helper functions that help unpack the contents of the messages passed to the `YFS` message processing methods.
  - `InsertOpenFile` and `InsertOFDeluxe` open file helper functions
  - `InsertFD` and `RemoveOpenFile` manipulate the open file list.
  - `SearchOpenFile` and `SearchByFD` look for files in the open file list.
  - `EditOpenFile` edits open file nodes
  - `RemoveMinFD` gets the minimum available `fd`
  - `MakeNewFile` initializes an inode structure for a new file/directory
  - `ReadFromBlock` reads a block and interacts with the block cache appropriately
  - `WriteToBlock` writes to a block and interacts with the block cache appropriately
  - `WriteINum` writes to an inode and interacts with the block and inode cache appropriately
  - `GetInodeAt` reads and returns an inode. Interacts with the block and inode cache appropriately
  - `GetFreeInode` and `GetFreeBlock` return free inodes and free blocks from the inode_bitmap and block_bitmap
  - `CreateDirEntry` creates a directory entry
  - `GetSectorNum` and `GetSectorPosition` help with getting the position of an inode on disk
  - `AddToBlock` adds data to a block `GetFromBlock` gets data from block
  - `AddBlockToInode` gives a block to an inode
  - `GetTrueBlock` gets a block from the `direct` field in an inode.
  - `CeilDiv` ceiling function
  - `CheckDir` checks that a directory contains no entries but `.` and `..`. Helped by `CheckDirHelper`
  - `FreeFileBlocks` frees the block associated with an `inode`.
  - `InsertBlockCache`, `GetBlockCache` block cache helper functions
  - `InsertInodeCache`, `GetInodeCache` inode cache helper functions 

## Testing

Tested using the test files in `clear/courses/comp421/pub/samples-lab3`. We also used the tests in the following GitHub repos: 
- https://github.com/pjgranahan/yfs-tests 
- https://github.com/cannontwo/Comp-421-Tests/tree/master/lab3/wcl2

We tested each of the methods in `iolib.a`, with the exception of `SymLink` and `ReadLink`. We did not implement symbolic links. Based on these tests, we believe that all of the other methods in `iolib.a` work correctly and as expected. 
