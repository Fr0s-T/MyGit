#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/branch.h"
#include "../include/colors.h"
#include "../include/helpers/file_io.h"

static int input_check(int argc, char **argv);
static int branch_list(void);
static const char *extract_branch_name(const char *head_ref);
static int should_skip_branch_entry(const char *name);
static void destroy_branch_names(char **names, int count);

int branch(int argc, char **argv) {
    if (input_check(argc, argv) != 0) {
        return (-1);
    }
    return (branch_list());
}

static int branch_list(void) {
    char *current_branch;
    const char *current_branch_name;
    char **names;
    struct dirent *entry;
    DIR *dir;
    int count;
    int status;

    current_branch = NULL;
    names = NULL;
    dir = NULL;
    count = 0;
    status = -1;

    if (file_io_read_first_line(".mygit/HEAD", &current_branch) != 0) {
        return (-1);
    }

    current_branch_name = extract_branch_name(current_branch);
    
    dir = opendir(".mygit/refs/heads");
    if (dir == NULL) {
        goto cleanup;
    }

    names = malloc(sizeof(char *));
    if (names == NULL) {
        goto cleanup;
    }

    while ((entry = readdir(dir)) != NULL) {

        if (should_skip_branch_entry(entry->d_name)) {
            continue;
        }

        names[count] = malloc(strlen(entry->d_name) + 1);
        if (names[count] == NULL) {
            goto cleanup;
        }

        strcpy(names[count], entry->d_name);
        count++;
        names = realloc(names, (count + 1) * sizeof(char *));

        if (names == NULL) {
            goto cleanup;
        }
    }

    if (current_branch_name != NULL) {
        printf( C_GREEN "*%s" C_RESET "\n", current_branch_name);
    }

    for (int i = 0; i < count; i++) {
        if (current_branch_name != NULL
                && strcmp(names[i], current_branch_name) == 0) {
           
                continue;
        }

        printf("  %s\n", names[i]);
    }
    status = 0;

cleanup:
    if (dir != NULL) {
        closedir(dir);
    }
    free(current_branch);
    destroy_branch_names(names, count);
    return (status);
}

static const char *extract_branch_name(const char *head_ref) {
    const char *last_slash;

    if (head_ref == NULL) {
        return (NULL);
    }
    last_slash = strrchr(head_ref, '/');
    if (last_slash == NULL || *(last_slash + 1) == '\0') {
        return (head_ref);
    }
    return (last_slash + 1);
}

static int should_skip_branch_entry(const char *name) {
    if (name == NULL) {
        return (1);
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return (1);
    }
    return (0);
}

static void destroy_branch_names(char **names, int count) {
    if (names == NULL) {
        return ;
    }
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
}

static int input_check(int argc, char **argv) {
    (void)argv;
    if (argc != 2) {
        printf("[branch] usage: mygit branch\n");
        return (-1);
    }
    return (0);
}
