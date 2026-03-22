#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "branch.h"
#include "colors.h"
#include "helpers/file_io.h"

static int input_check(int argc, char **argv, char *b_name);
static void print_branch_usage(void);
static int branch_list(void);
static const char *extract_branch_name(const char *head_ref);
static int should_skip_branch_entry(const char *name);
static int create_branch(char *name);
static int delete_branch(char *name);

int branch(int argc, char **argv) {
    char name[17];
    int input_status;

    input_status = input_check(argc, argv, name);
    if (input_status == -1) {
        return (-1);
    }

    if (input_status == 1) {
        return branch_list();
    }
    
    if (input_status == 2) {
        return create_branch(name);
    }
    if (input_status == 3) {
        return delete_branch(name);
    }

    return (0);
}

static int create_branch(char *name) {
    char *current_branch;
    char *new_branch_path;
    char **names;
    int count;
    int stat;

    stat = -1;
    current_branch = NULL;
    new_branch_path = NULL;
    names = NULL;
    count = 0;

    if (branch_load_names(&names, &count) != 0) {
        return (-1);
    }

    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            printf("[branch] branch '%s' already exists\n", name);
            goto cleanup;
        }
    }

    if (file_io_read_first_line(".mygit/HEAD", &current_branch) != 0) {
        goto cleanup;
    }

    new_branch_path = malloc(strlen(".mygit/refs/heads/") + strlen(name) + 1);
    if (new_branch_path == NULL) {
        goto cleanup;
    }

    sprintf(new_branch_path, ".mygit/refs/heads/%s", name);

    if (file_io_copy_file(current_branch, new_branch_path) != 0) {
        printf("[Branch/Create] failed to create branch\n");
        goto cleanup;
    }

    if (strcmp(name, "does_it_work") == 0) {
        printf("[branch] created '%s'\nyes it does.\n", name);
    }
    stat = 0;

cleanup:
    free(current_branch);
    free(new_branch_path);
    branch_destroy_names(names, count);
    return (stat);
}

static int delete_branch(char *name) {
    char *current_branch;
    const char *current_branch_name;
    char *branch_path;
    char **names;
    int count;
    int exists;
    int status;

    current_branch = NULL;
    current_branch_name = NULL;
    branch_path = NULL;
    names = NULL;
    count = 0;
    exists = 0;
    status = -1;

    if (branch_load_names(&names, &count) != 0) {
        return (-1);
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            exists = 1;
            break;
        }
    }
    if (exists == 0) {
        printf("[branch] branch '%s' does not exist\n", name);
        goto cleanup;
    }
    if (file_io_read_first_line(".mygit/HEAD", &current_branch) != 0) {
        goto cleanup;
    }
    current_branch_name = extract_branch_name(current_branch);
    if (current_branch_name != NULL && strcmp(current_branch_name, name) == 0) {
        printf("[branch] cannot delete the current branch '%s'\n", name);
        goto cleanup;
    }
    branch_path = malloc(strlen(".mygit/refs/heads/") + strlen(name) + 1);
    if (branch_path == NULL) {
        goto cleanup;
    }
    sprintf(branch_path, ".mygit/refs/heads/%s", name);
    if (remove(branch_path) != 0) {
        printf("[branch] failed to delete '%s'\n", name);
        goto cleanup;
    }
    printf("[branch] deleted '%s'\n", name);
    status = 0;

cleanup:
    free(current_branch);
    free(branch_path);
    branch_destroy_names(names, count);
    return (status);
}


static int branch_list(void) {
    char *current_branch;
    const char *current_branch_name;
    char **names;
    int count;
    int status;

    current_branch = NULL;
    names = NULL;
    count = 0;
    status = -1;

    if (file_io_read_first_line(".mygit/HEAD", &current_branch) != 0) {
        return (-1);
    }

    current_branch_name = extract_branch_name(current_branch);

    if (branch_load_names(&names, &count) != 0) {
        goto cleanup;
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
    free(current_branch);
    branch_destroy_names(names, count);
    return (status);
}

int branch_load_names(char ***names_out, int *count_out) {
    char **names;
    char **new_names;
    struct dirent *entry;
    DIR *dir;
    int count;

    if (names_out == NULL || count_out == NULL) {
        return (-1);
    }
    *names_out = NULL;
    *count_out = 0;
    dir = opendir(".mygit/refs/heads");
    if (dir == NULL) {
        return (-1);
    }
    names = NULL;
    count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (should_skip_branch_entry(entry->d_name)) {
            continue;
        }
        new_names = realloc(names, (count + 1) * sizeof(char *));
        if (new_names == NULL) {
            closedir(dir);
            branch_destroy_names(names, count);
            return (-1);
        }
        names = new_names;
        names[count] = malloc(strlen(entry->d_name) + 1);
        if (names[count] == NULL) {
            closedir(dir);
            branch_destroy_names(names, count);
            return (-1);
        }
        strcpy(names[count], entry->d_name);
        count++;
    }
    closedir(dir);
    *names_out = names;
    *count_out = count;
    return (0);
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

void branch_destroy_names(char **names, int count) {
    if (names == NULL) {
        return ;
    }
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
}

static int input_check(int argc, char **argv, char *b_name) {
    if (argc < 2) {
        print_branch_usage();
        return (-1);
    }
    if (strcmp(argv[1], "branch") != 0) {
        return -1;
    }
    if (argc == 2) {
        return (1);
    }
    if (argc == 3) {
        if (strlen(argv[2]) > 16) {
            printf("[branch] Max allowed len for a branch name is 16 char\n");
            return (-1);
        }
        strcpy(b_name, argv[2]);
        return (2);
    }
    if (argc == 4 && strcmp(argv[2], "-d") == 0) {
        if (strlen(argv[3]) > 16) {
            printf("[branch] Max allowed len for a branch name is 16 char\n");
            return (-1);
        }
        strcpy(b_name, argv[3]);
        return (3);
    }

    print_branch_usage();
    return (-1);
}

static void print_branch_usage(void) {
    printf("[branch] usage: mygit branch\n");
    printf("\tmygit branch branch_name to create a branch\n");
    printf("\tmygit branch -d branch_name to delete a branch\n");
}
