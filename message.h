struct message {
    int type;
    pid_t pid;
    int retval;
    char text[16];
    short cd;
    short cd2;
};

// Use for Open/Close/Create
struct messageSinglePathName {
    int type;
    pid_t pid;
    int retval;
    void *pathname;
    short cd;
    char pad[10];
}

// 
struct messageDoublePathName {
    int type;
    pid_t pid;
    int retval;
    void *pathname1;
    void *pathname2;
    short cd1;
    short cd2;
}

// Use for Close Read Write
struct messageFDSizeBuf {
    int type;
    pid_t pid;
    int retval;
    int fd;
    int size;
    void *buf;
    char pad[4];
}

struct messageSeek {
    int type;
    pid_t pid;
    int retval;
    int fd;
    int offset;
    int whence;
}

/**
 * @brief returns a new char * with a null terminated version of the input string.
 * 
 * @param str the string to turn into a null terminated string
 * @param max_len the max len of the input string
 * @return char* a new char * with a null terminated version of the input string.
 */
char *
MakeNullTerminated(char *str, int max_len) {
    int i;
    for (i = 0; i < max_len; i++) {
        if (str[i] == '\0')
            break;
    }
    if (i == (max_len - 1) && str[i] != '\0') {
        char *new_str = malloc(max_len + 1);
        int j;
        for (j = 0; j < max_len; j++) {
            new_str[j] = str[j];
        }
        new_str[max_len] = '\0';
        return new_str;
    }
    char *new_str = malloc(i + 1);
    int j;
    for (j = 0; j <= i; j++) {
        new_str[j] = str[j];
    }
    return new_str;
}