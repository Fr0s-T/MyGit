#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/my_includes.h"

/*
** Internal helpers used only by init.c.
** They keep init() readable by hiding repeated:
** - path generation
** - directory creation
** - file creation
*/
static void failed_msg_printer(const char *name);
static int create_subdirectory(const char *base, const char *name);
static int create_file_in_directory(const char *base, const char *name);

int init(int argc) {
    if (argc > 2) {
        printf("\nToo many args\n");
        return -1;
    }

    /*
    ** Step 1: create the repository root directory.
    */
    if (create_directory(".mygit") == -1) {
        failed_msg_printer(".mygit");
        return -1;
    }

    /*
    ** Step 2: resolve the absolute path of .mygit.
    ** This becomes the base path for the rest of the repo structure.
    */
    char *repo_path = realpath(".mygit", NULL);
    if (repo_path == NULL) {
        perror("realpath");
        return -1;
    }

    /*
    ** Step 3: create .mygit/objects and .mygit/refs
    */
    if (create_subdirectory(repo_path, "objects") == -1) {
        free(repo_path);
        return -1;
    }

    if (create_subdirectory(repo_path, "refs") == -1) {
        free(repo_path);
        return -1;
    }

    /*
    ** Step 4: create .mygit/refs/heads
    */
    char *refs_path = generate_path(repo_path, "refs");
    if (refs_path == NULL) {
        free(repo_path);
        return -1;
    }

    if (create_subdirectory(refs_path, "heads") == -1) {
        free(refs_path);
        free(repo_path);
        return -1;
    }

    /*
    ** Step 5: create empty files:
    ** - .mygit/refs/heads/main
    ** - .mygit/index
    ** - .mygit/HEAD
    */
    char *heads_path = generate_path(refs_path, "heads");
    if (heads_path == NULL) {
        free(refs_path);
        free(repo_path);
        return -1;
    }

    if (create_file_in_directory(heads_path, "main") == -1) {
        free(heads_path);
        free(refs_path);
        free(repo_path);
        return -1;
    }

    free(heads_path);
    free(refs_path);

    if (create_file_in_directory(repo_path, "index") == -1) {
        free(repo_path);
        return -1;
    }

    if (create_file_in_directory(repo_path, "HEAD") == -1) {
        free(repo_path);
        return -1;
    }

    free(repo_path);
    return 0;
}

/*
** Creates a child directory inside a base path.
** Example:
** base = "/tmp/.mygit", name = "objects"
** result => "/tmp/.mygit/objects"
*/
static int create_subdirectory(const char *base, const char *name) {
    char *path = generate_path(base, name);

    if (path == NULL) {
        return -1;
    }

    if (create_directory(path) == -1) {
        failed_msg_printer(path);
        free(path);
        return -1;
    }

    free(path);
    return 0;
}

/*
** Creates an empty file inside a base directory.
** Example:
** base = "/tmp/.mygit", name = "HEAD"
** result => "/tmp/.mygit/HEAD"
*/
static int create_file_in_directory(const char *base, const char *name) {
    char *path = generate_path(base, name);

    if (path == NULL) {
        return -1;
    }

    if (create_empty_file(path) == -1) {
        failed_msg_printer(path);
        free(path);
        return -1;
    }

    free(path);
    return 0;
}

/*
** Small local logger for failed init operations.
*/
static void failed_msg_printer(const char *name) {
    printf("\nFailed on: %s\n", name);
}
