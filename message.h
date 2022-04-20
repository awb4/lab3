struct message {
    int type;
    pid_t pid;
    int retval;
    char text[16];
    short cd;
    short cd2;
};

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