#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/hardware.h>


int
main(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    int err;
    int err2;
    printf("Tcreate\n");
    write(2, "A\n", 2);
    TracePrintf(0, "Written\n");
    MkDir("/foo");
    err = Create("/foo/bar");
    fprintf(stderr, "Create returned %d\n", err);
    err2 = Create("/foo/zzz");
    fprintf(stderr, "Create returned %d\n", err2);
    fprintf(stderr, "%d", Open("/foo/zzz"));

    Sync();
    Delay(3);
    fprintf(stderr, "Done with Sync\n");

/*	Shutdown(); */

    exit(0);
}
