#include <comp421/filesystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <comp421/iolib.h>
#include <comp421/hardware.h>

struct message {
    int type;
    char text[24];
};

struct message *message;
struct fs_header header;

int 
main(char *argc, char **argv) 
{
    if (Fork() == 0) {
        Exec(argv[1], argv + 1);
    } else {
        while(1) {
            pid_t pid = Receive(*message);
            /* Construct and overwrite the message to reply */
            Reply(*message, pid);
        }
    }
}
