struct message {
    int type;
    pid_t pid;
    int retval;
    char text[20];
};