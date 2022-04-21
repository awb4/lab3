#include <stdio.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>

/* After running this, try topen2 and/or tunlink2 */

int
main(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	printf("\n%d\n\n", Create("/foo"));
	printf("\n%d\n\n", Create("/bar"));
	printf("\n%d\n\n", Create("/foo"));
	printf("\n%d\n\n", Create("/foo/zzz"));

	Shutdown();
	return 0;
}
