#include <stdio.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>

int
main(int argc, char **argv)
{
	int status;
	(void) argc;
	(void) argv;
	status = Create("/a");
	printf("Create status %d\n", status);

	status = Link("/a", "/b");
	printf("Link status %d\n", status);

	status = Unlink("/b");
	printf("Unlink status %d\n", status);

	Shutdown();
	return 0;
}
