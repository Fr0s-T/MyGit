#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "add.h"
#include "add_snapshot.h"
#include "add_creating_blob_and_indexing.h"

static int input_check(int argc, char **argv);
static int repo_is_initialized(void);

int add(int argc, char **argv) {
    char cwd[PATH_MAX];
    file_data **files = NULL;
    int len_files = 0;
    int status;

    if (input_check(argc, argv) == -1) {
        return -1;
    }
    if (repo_is_initialized() == 0) {
        printf("[add] repository is not initialized\n");
        printf("[add] call `mygit init` first\n");
        return -1;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("[add] failed to get working dir\n");
        return -1;
    }

    if (add_collect_changed_files(cwd, &files, &len_files) == -1) {
        printf("[add] failed to collect changed files\n");
        add_destroy_file_list(files, len_files);
        return -1;
    }
    if (len_files == 0) {
        printf("[add] nothing changed\n");
        add_destroy_file_list(files, len_files);
        return 0;
    }

    status = create_blob_and_indexing(files, len_files, cwd);
    add_destroy_file_list(files, len_files);
    if (status != 0) {
        printf("[add] create_blob_and_indexing failed\n");
        return -1;
    }
    printf("[add] added %d file(s)\n", len_files);
    return 0;
}

static int input_check(int argc, char **argv) {
    if (argc > 3) {
        printf("\nToo many args\n");
        return -1;
    }

    if (argc < 3) {
        printf("\nMissing add target\n");
        return -1;
    }

    if (strcmp(argv[2], ".") == 0) {
        return 0;
    }

    printf("\nNot supported or wrong arg\n");
    return -1;
}

static int repo_is_initialized(void) {
    if (access(".mygit", F_OK) != 0) {
        return (0);
    }
    if (access(".mygit/index", F_OK) != 0) {
        return (0);
    }
    if (access(".mygit/HEAD", F_OK) != 0) {
        return (0);
    }
    if (access(".mygit/objects", F_OK) != 0) {
        return (0);
    }
    if (access(".mygit/refs/heads", F_OK) != 0) {
        return (0);
    }
    return (1);
}
