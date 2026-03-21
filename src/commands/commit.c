#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "colors.h"
#include "commit.h"
#include "hash.h"
#include "helpers/commit_object.h"
#include "helpers/commit_tree.h"
#include "services.h"

#define COMMIT_TAG_COLOR C_BLUE
#define COMMIT_VALUE_COLOR C_CYAN
#define COMMIT_SUCCESS_COLOR C_GREEN
#define COMMIT_ERROR_COLOR C_RED

static char *duplicate_string(const char *value);
static int input_check(int argc, char **argv, char **commit_msg);

int commit(int argc, char **argv) {
    char cwd[PATH_MAX];
    char commit_hash[SHA1_HEX_BUFFER_SIZE];
    node *root;
    char *temp_path;
    char *index_path;
    char *commit_msg;
    int accept_status;
    int status;

    root = NULL;
    temp_path = NULL;
    index_path = NULL;
    commit_msg = NULL;
    status = -1;
    if (input_check(argc, argv, &commit_msg) == -1) {
        return (-1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf(COMMIT_TAG_COLOR "[commit]" C_RESET " "
            COMMIT_ERROR_COLOR "failed to get working dir\n" C_RESET);
        goto cleanup;
    }
    temp_path = generate_path(cwd, ".mygit");
    if (temp_path == NULL) {
        printf(COMMIT_TAG_COLOR "[commit]" C_RESET " "
            COMMIT_ERROR_COLOR "failed to build .mygit path\n" C_RESET);
        goto cleanup;
    }
    index_path = generate_path(temp_path, "index");
    if (index_path == NULL) {
        printf(COMMIT_TAG_COLOR "[commit]" C_RESET " "
            COMMIT_ERROR_COLOR "failed to build index path\n" C_RESET);
        goto cleanup;
    }
    if (build_tree_from_index_path(index_path, 1, &root) != 0) {
        printf(COMMIT_TAG_COLOR "[commit]" C_RESET " "
            COMMIT_ERROR_COLOR "failed to build tree from index\n" C_RESET);
        goto cleanup;
    }
    accept_status = accept_commit(root, commit_msg, commit_hash);
    if (accept_status == COMMIT_NOTHING_TO_DO) {
        printf(COMMIT_TAG_COLOR "[commit]" C_RESET " "
            COMMIT_ERROR_COLOR "nothing has changed, commit aborted\n" C_RESET);
        status = 0;
        goto cleanup;
    }
    if (accept_status != 0) {
        printf(COMMIT_TAG_COLOR "[commit]" C_RESET " "
            COMMIT_ERROR_COLOR "accept_commit failed\n" C_RESET);
        goto cleanup;
    }
    printf(COMMIT_TAG_COLOR "[commit]" C_RESET " "
        COMMIT_SUCCESS_COLOR "root hash: "
        COMMIT_VALUE_COLOR "%s\n" C_RESET, root->hash);
    printf(COMMIT_TAG_COLOR "[commit]" C_RESET " "
        COMMIT_SUCCESS_COLOR "commit hash: "
        COMMIT_VALUE_COLOR "%s\n" C_RESET, commit_hash);
    status = 0;

cleanup:
    free(commit_msg);
    free(index_path);
    free(temp_path);
    node_destroy(root);
    return (status);
}

static int input_check(int argc, char **argv, char **commit_msg) {
    if (argc != 4) {
        printf("[commit/input_check] usage: mygit commit -m \"message\"\n");
        return (-1);
    }
    if (strcmp(argv[2], "-m") != 0) {
        printf("[commit/input_check] missing required flag: -m\n");
        return (-1);
    }
    if (argv[3] == NULL || argv[3][0] == '\0') {
        printf("[commit/input_check] commit message is empty\n");
        return (-1);
    }
    *commit_msg = duplicate_string(argv[3]);
    if (*commit_msg == NULL) {
        return (-1);
    }
    return (0);
}

static char *duplicate_string(const char *value) {
    char *copy;

    if (value == NULL) {
        return (NULL);
    }
    copy = malloc(strlen(value) + 1);
    if (copy == NULL) {
        return (NULL);
    }
    strcpy(copy, value);
    return (copy);
}
