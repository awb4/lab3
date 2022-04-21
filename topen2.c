#include <stdio.h>
#include <stdlib.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>

/* Try tcreate2 before this, or try this just by itself */

int
main(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	Create("/foo");
	Create("/bar");
	Create("/foo/zzz");
	TracePrintf(0, "topen2: \n%d\n\n", Open("/foo"));
	TracePrintf(0, "topen2: \n%d\n\n", Open("/bar"));
	TracePrintf(0, "topen2: \n%d\n\n", Open("/foo"));
	TracePrintf(0, "topen2: \n%d\n\n", Open("/foo/zzz"));
	Shutdown();
	return 0;
}
