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
    printf("Tcreate\n");
    write(2, "A\n", 2);
    TracePrintf(0, "Written\n");
    err = Create("/foo");
    fprintf(stderr, "Create returned %d\n", err);

    Sync();
    Delay(3);
    fprintf(stderr, "Done with Sync\n");

/*	Shutdown(); */

    exit(0);
}
