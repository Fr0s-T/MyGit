#include <stdio.h>
#include <string.h>

#include "../include/add.h"
#include "../include/commit.h"
#include "../include/init.h"

int router(int argc, char **argv);

int main(int argc, char **argv) {
    if (router(argc, argv) != 0) {
        printf("\nfatal error\n");
        return -1;
    }

    return 0;
}

int router(int argc, char **argv) {
    if (argc == 1) {
        printf("\nNo command has been enterd");
        return -1;
    }
    else if (strcmp(argv[1], "init") == 0) {
        printf("\nInit has been called. Initilizing local git repo\n");
        return init(argc);
    }
    else if (strcmp(argv[1], "add") == 0) {
        printf("\nAdd has been called. Tracking changes...\n");
        return add(argc, argv);
    }
    else if (strcmp(argv[1], "commit") == 0) {
        printf("commit has been called\n");
        return commit(argc, argv);
    }
    return -1;
}
