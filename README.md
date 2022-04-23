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

### `iolib.a`

### `yfs`


## Testing

Tested using the test files in `clear/courses/comp421/pub/samples-lab3`. We also used the tests in the following GitHub repos: 
- https://github.com/pjgranahan/yfs-tests 
- https://github.com/cannontwo/Comp-421-Tests/tree/master/lab3/wcl2

We tested each of the calls in `iolib.a`, with the exception of `SymLink` and `ReadLink`. We did not implement symbolic links. Based on these tests, we believe that all of the other methods in `iolib.a` work correctly and as expected. 
