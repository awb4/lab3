#include <stdio.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>

/* Try tcreate2 before this, or try this just by itself */

int
main(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	printf("\n%d\n\n", Unlink("/bar"));
	printf("\n%d\n\n", Unlink("/bar"));
	printf("\n%d\n\n", Unlink("/foo/abc"));
	printf("\n%d\n\n", Unlink("/foo"));

	Shutdown();
	return 0;
}
