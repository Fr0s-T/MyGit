#include "my_includes.h"
#include <errno.h>

#include "checkout_prepare.h"
#include "checkout_index.h"
static void destroy_checkout_entries(checkout_entry **entries, int count);
static void print_checkout_entries(const char *label,
    checkout_entry **entries, int count);
int build_survivng_entries(checkout_entry ***surviving_entrys,
    checkout_entry **target_entrys, checkout_entry **current_entrys,
    int target_count, int current_count, int *survivng_count);
int purge_non_surviving_current_entries(checkout_entry **current_entries,
    int current_count, checkout_entry **surviving_entries,
    int surviving_count);
int materialize_target_entries(checkout_entry **target_entries, int target_count);
int rewrite_index_from_checkout_entries(checkout_entry **entries, int count);
static int checkout_entry_is_surviving(checkout_entry *entry,
    checkout_entry **surviving_entries, int surviving_count);
static void remove_empty_parent_directories(const char *relative_path);
static int ensure_parent_directories(const char *full_path);
static int checkout_validate_entries_available(checkout_entry **entries, int count);
static int restore_checkout_state(checkout_entry **current_entries,
    int current_count, checkout_entry **target_entries, int target_count,
    checkout_entry **surviving_entries, int surviving_count);
static int checkout_target_conflicts_with_untracked(checkout_entry **target_entries,
    int target_count, checkout_entry **current_entries, int current_count);
static int checkout_path_is_tracked(const char *relative_path,
    checkout_entry **entries, int count);

int checkout(int argc, char **argv) {
    char *branch_name;
    char *current_ref_path;
    char *target_ref_path;
    char *target_commit_hash;
    char *target_root_hash;
    char *target_root_path;
    checkout_entry **current_entries;
    checkout_entry **target_entries;
    checkout_entry **survivg_entires;
    node *root;
    int current_entry_count;
    int target_entry_count;
    int surviving_count;
    int conflict_status;
    int status;

    branch_name = NULL;
    current_ref_path = NULL;
    target_ref_path = NULL;
    target_commit_hash = NULL;
    target_root_hash = NULL;
    target_root_path = NULL;
    current_entries = NULL;
    target_entries = NULL;
    survivg_entires = NULL;
    root = NULL;
    current_entry_count = 0;
    target_entry_count = 0;
    surviving_count = 0;
    conflict_status = 0;
    status = -1;
    if (checkout_input_check(argc, argv, &branch_name) != 0) {
        return (-1);
    }
    if (checkout_branch_exists(branch_name) == 0) {
        printf("[checkout] branch '%s' does not exist\n", branch_name);
        return (-1);
    }
    if (file_io_read_first_line(".mygit/HEAD", &current_ref_path) != 0) {
        printf("[checkout] failed to read current HEAD\n");
        return (-1);
    }
    if (checkout_repo_is_up_to_date_with_branch() != 0) {
        printf("[checkout] add and commit then try to checkout\n");
        goto cleanup;
    }
    if (checkout_read_target_commit(branch_name,
            &target_ref_path, &target_commit_hash) != 0) {
        printf("[checkout] failed to read target branch '%s'\n", branch_name);
        goto cleanup;
    }
    if (checkout_read_target_root(target_commit_hash,
            &target_root_hash, &target_root_path) != 0) {
        printf("[checkout] failed to read target root for '%s'\n", branch_name);
        goto cleanup;
    }
    if (checkout_collect_current_tracked_entries(&current_entries,
            &current_entry_count) != 0) {
        printf("[checkout] failed to read current tracked entries\n");
        goto cleanup;
    }
    printf("[checkout] branch '%s' exists\n", branch_name);
    printf("[checkout] repo is up to date with current branch\n");
    printf("[checkout] target ref: %s\n", target_ref_path);
    printf("[checkout] target commit: %s\n", target_commit_hash);
    printf("[checkout] target root hash: %s\n", target_root_hash);
    printf("[checkout] target root path: %s\n", target_root_path);
    root = node_create("", target_root_hash, NODE_ROOT, NULL);
    if (root == NULL) {
        printf("[checkout] failed to create root node\n");
        goto cleanup;
    }
    if (target_root_path != NULL) {
        if (checkout_generate_tree(root, target_root_path, &target_entries,
                &target_entry_count) != 0) {
            printf("[checkout] failed to build target tree\n");
            goto cleanup;
        }
    }
    print_tree(root, 0);
    print_checkout_entries("[checkout] current tracked entries:",
        current_entries, current_entry_count);
    print_checkout_entries("[checkout] target tracked entries:",
        target_entries, target_entry_count);
    if (build_survivng_entries(&survivg_entires, target_entries,
            current_entries, target_entry_count, current_entry_count,
            &surviving_count) != 0) {
        printf("[checkout] failed to build surviving entries\n");
        goto cleanup;
    }
    print_checkout_entries("[checkout] surviving tracked entries:",
        survivg_entires, surviving_count);
    if (checkout_validate_entries_available(current_entries,
            current_entry_count) != 0) {
        printf("[checkout] current tracked objects are incomplete\n");
        goto cleanup;
    }
    if (checkout_validate_entries_available(target_entries,
            target_entry_count) != 0) {
        printf("[checkout] target tracked objects are incomplete\n");
        goto cleanup;
    }
    conflict_status = checkout_target_conflicts_with_untracked(target_entries,
        target_entry_count, current_entries, current_entry_count);
    if (conflict_status < 0) {
        printf("[checkout] failed to check untracked conflicts\n");
        goto cleanup;
    }
    if (conflict_status > 0) {
        printf("[checkout] target branch would overwrite untracked files\n");
        goto cleanup;
    }
    if (purge_non_surviving_current_entries(current_entries, current_entry_count,
            survivg_entires, surviving_count) != 0) {
        printf("[checkout] failed to purge current tracked entries\n");
        if (restore_checkout_state(current_entries, current_entry_count,
                target_entries, target_entry_count, survivg_entires,
                surviving_count) != 0) {
            printf("[checkout] rollback failed after purge\n");
        }
        goto cleanup;
    }
    if (materialize_target_entries(target_entries, target_entry_count) != 0) {
        printf("[checkout] failed to materialize target entries\n");
        if (restore_checkout_state(current_entries, current_entry_count,
                target_entries, target_entry_count, survivg_entires,
                surviving_count) != 0) {
            printf("[checkout] rollback failed after materialize\n");
        }
        goto cleanup;
    }
    if (rewrite_index_from_checkout_entries(target_entries,
            target_entry_count) != 0) {
        printf("[checkout] failed to rewrite index\n");
        if (restore_checkout_state(current_entries, current_entry_count,
                target_entries, target_entry_count, survivg_entires,
                surviving_count) != 0) {
            printf("[checkout] rollback failed after index rewrite\n");
        }
        goto cleanup;
    }
    if (checkout_update_head_ref(target_ref_path) != 0) {
        printf("[checkout] failed to update HEAD\n");
        if (restore_checkout_state(current_entries, current_entry_count,
                target_entries, target_entry_count, survivg_entires,
                surviving_count) != 0) {
            printf("[checkout] rollback failed after HEAD update\n");
        }
        if (current_ref_path != NULL
                && checkout_update_head_ref(current_ref_path) != 0) {
            printf("[checkout] failed to restore HEAD\n");
        }
        goto cleanup;
    }
    status = 0;

cleanup:
    destroy_checkout_entries(current_entries, current_entry_count);
    destroy_checkout_entries(target_entries, target_entry_count);
    destroy_checkout_entries(survivg_entires, surviving_count);
    node_destroy(root);
    free(target_root_path);
    free(target_root_hash);
    free(target_commit_hash);
    free(target_ref_path);
    free(current_ref_path);
    return (status);
}

static void destroy_checkout_entries(checkout_entry **entries, int count) {
    if (entries == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        checkout_entry_destroy(entries[i]);
    }
    free(entries);
}

static void print_checkout_entries(const char *label,
    checkout_entry **entries, int count) {
    if (entries == NULL || count <= 0) {
        return;
    }
    printf("%s\n\n", label);
    for (int i = 0; i < count; i++) {
        if (entries[i] == NULL) {
            continue;
        }
        printf("path: %s\n", entries[i]->relative_path);
        printf("hash: %s\n", entries[i]->blob_hash);
        printf("object: %s\n\n", entries[i]->object_path);
    }
}

int build_survivng_entries(checkout_entry ***surviving_entrys,
    checkout_entry **target_entrys, checkout_entry **current_entrys,
    int target_count, int current_count, int *survivng_count) {
        
    checkout_entry **new_entries;
    checkout_entry *new_entry;

    if (surviving_entrys == NULL || survivng_count == NULL
            || target_count < 0 || current_count < 0) {
        return (-1);
    }
    *surviving_entrys = NULL;
    *survivng_count = 0;
    if ((target_count > 0 && target_entrys == NULL)
            || (current_count > 0 && current_entrys == NULL)) {
        return (-1);
    }
    for (int i = 0; i < target_count; i++) {
        for (int j = 0; j < current_count; j++) {
            if (checkout_entry_compare(target_entrys[i], current_entrys[j]) != 0) {
                continue;
            }
            new_entries = realloc(*surviving_entrys,
                (size_t)(*survivng_count + 1) * sizeof(checkout_entry *));
            if (new_entries == NULL) {
                destroy_checkout_entries(*surviving_entrys, *survivng_count);
                *surviving_entrys = NULL;
                *survivng_count = 0;
                return (-1);
            }
            *surviving_entrys = new_entries;
            new_entry = checkout_entry_create(target_entrys[i]->relative_path,
                target_entrys[i]->blob_hash);
            if (new_entry == NULL) {
                destroy_checkout_entries(*surviving_entrys, *survivng_count);
                *surviving_entrys = NULL;
                *survivng_count = 0;
                return (-1);
            }
            (*surviving_entrys)[*survivng_count] = new_entry;
            (*survivng_count)++;
            break;
        }
    }
    return (0);
}

int purge_non_surviving_current_entries(checkout_entry **current_entries,
    int current_count, checkout_entry **surviving_entries,
    int surviving_count) {
    char cwd[PATH_MAX];
    char *full_path;

    if (current_count < 0) {
        return (-1);
    }
    if (current_count == 0) {
        return (0);
    }
    if (current_entries == NULL) {
        return (-1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return (-1);
    }
    for (int i = 0; i < current_count; i++) {
        if (current_entries[i] == NULL || current_entries[i]->relative_path == NULL) {
            return (-1);
        }
        if (checkout_entry_is_surviving(current_entries[i], surviving_entries,
                surviving_count) != 0) {
            continue;
        }
        full_path = generate_path(cwd, current_entries[i]->relative_path);
        if (full_path == NULL) {
            return (-1);
        }
        if (remove(full_path) != 0) {
            if (errno == ENOENT) {
                free(full_path);
                remove_empty_parent_directories(current_entries[i]->relative_path);
                continue;
            }
            free(full_path);
            return (-1);
        }
        free(full_path);
        remove_empty_parent_directories(current_entries[i]->relative_path);
    }
    return (0);
}

int materialize_target_entries(checkout_entry **target_entries, int target_count) {
    char cwd[PATH_MAX];
    char *full_path;

    if (target_count < 0) {
        return (-1);
    }
    if (target_count == 0) {
        return (0);
    }
    if (target_entries == NULL) {
        return (-1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return (-1);
    }
    for (int i = 0; i < target_count; i++) {
        if (target_entries[i] == NULL || target_entries[i]->relative_path == NULL
                || target_entries[i]->object_path == NULL) {
            return (-1);
        }
        full_path = generate_path(cwd, target_entries[i]->relative_path);
        if (full_path == NULL) {
            return (-1);
        }
        if (ensure_parent_directories(full_path) != 0) {
            free(full_path);
            return (-1);
        }
        if (file_io_copy_file(target_entries[i]->object_path, full_path) != 0) {
            free(full_path);
            return (-1);
        }
        free(full_path);
    }
    return (0);
}

int rewrite_index_from_checkout_entries(checkout_entry **entries, int count) {
    FILE *temp_file;
    char cwd[PATH_MAX];
    char index_path[PATH_MAX];
    char temp_path[PATH_MAX];

    if (count < 0) {
        return (-1);
    }
    if (count > 0 && entries == NULL) {
        return (-1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return (-1);
    }
    if (snprintf(index_path, sizeof(index_path), "%s/.mygit/index", cwd)
            >= (int)sizeof(index_path)) {
        return (-1);
    }
    if (snprintf(temp_path, sizeof(temp_path), "%s/.mygit/index.tmp", cwd)
            >= (int)sizeof(temp_path)) {
        return (-1);
    }
    temp_file = fopen(temp_path, "w");
    if (temp_file == NULL) {
        return (-1);
    }
    for (int i = 0; i < count; i++) {
        if (entries[i] == NULL || entries[i]->relative_path == NULL
                || entries[i]->blob_hash == NULL) {
            fclose(temp_file);
            remove(temp_path);
            return (-1);
        }
        if (fprintf(temp_file, "%s\t%s\n", entries[i]->relative_path,
                entries[i]->blob_hash) < 0) {
            fclose(temp_file);
            remove(temp_path);
            return (-1);
        }
    }
    if (fclose(temp_file) != 0) {
        remove(temp_path);
        return (-1);
    }
    if (rename(temp_path, index_path) != 0) {
        remove(temp_path);
        return (-1);
    }
    return (0);
}

static int checkout_validate_entries_available(checkout_entry **entries, int count) {
    if (count < 0) {
        return (-1);
    }
    if (count == 0) {
        return (0);
    }
    if (entries == NULL) {
        return (-1);
    }
    for (int i = 0; i < count; i++) {
        if (entries[i] == NULL || entries[i]->object_path == NULL) {
            return (-1);
        }
        if (access(entries[i]->object_path, F_OK) != 0) {
            return (-1);
        }
    }
    return (0);
}

static int restore_checkout_state(checkout_entry **current_entries,
    int current_count, checkout_entry **target_entries, int target_count,
    checkout_entry **surviving_entries, int surviving_count) {
    if (purge_non_surviving_current_entries(target_entries, target_count,
            surviving_entries, surviving_count) != 0) {
        return (-1);
    }
    if (materialize_target_entries(current_entries, current_count) != 0) {
        return (-1);
    }
    if (rewrite_index_from_checkout_entries(current_entries, current_count) != 0) {
        return (-1);
    }
    return (0);
}

static int checkout_target_conflicts_with_untracked(checkout_entry **target_entries,
    int target_count, checkout_entry **current_entries, int current_count) {
    char cwd[PATH_MAX];

    if (target_count < 0 || current_count < 0) {
        return (-1);
    }
    if (target_count == 0) {
        return (0);
    }
    if (target_entries == NULL) {
        return (-1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return (-1);
    }
    for (int i = 0; i < target_count; i++) {
        char *full_path;

        if (target_entries[i] == NULL || target_entries[i]->relative_path == NULL) {
            return (-1);
        }
        if (checkout_path_is_tracked(target_entries[i]->relative_path,
                current_entries, current_count) != 0) {
            continue;
        }
        full_path = generate_path(cwd, target_entries[i]->relative_path);
        if (full_path == NULL) {
            return (-1);
        }
        if (access(full_path, F_OK) == 0) {
            free(full_path);
            return (1);
        }
        if (errno != ENOENT) {
            free(full_path);
            return (-1);
        }
        free(full_path);
    }
    return (0);
}

static int checkout_path_is_tracked(const char *relative_path,
    checkout_entry **entries, int count) {
    if (relative_path == NULL || count < 0) {
        return (0);
    }
    if (count == 0) {
        return (0);
    }
    if (entries == NULL) {
        return (0);
    }
    for (int i = 0; i < count; i++) {
        if (entries[i] == NULL || entries[i]->relative_path == NULL) {
            continue;
        }
        if (strcmp(entries[i]->relative_path, relative_path) == 0) {
            return (1);
        }
    }
    return (0);
}

static int checkout_entry_is_surviving(checkout_entry *entry,
    checkout_entry **surviving_entries, int surviving_count) {
    if (entry == NULL) {
        return (0);
    }
    for (int i = 0; i < surviving_count; i++) {
        if (surviving_entries == NULL || surviving_entries[i] == NULL) {
            continue;
        }
        if (checkout_entry_compare(entry, surviving_entries[i]) == 0) {
            return (1);
        }
    }
    return (0);
}

static void remove_empty_parent_directories(const char *relative_path) {
    char *directory_path;
    char *last_slash;

    if (relative_path == NULL) {
        return;
    }
    directory_path = malloc(strlen(relative_path) + 1);
    if (directory_path == NULL) {
        return;
    }
    strcpy(directory_path, relative_path);
    last_slash = strrchr(directory_path, '/');
    while (last_slash != NULL) {
        *last_slash = '\0';
        if (rmdir(directory_path) != 0) {
            break;
        }
        last_slash = strrchr(directory_path, '/');
    }
    free(directory_path);
}

static int ensure_parent_directories(const char *full_path) {
    char *path_copy;
    char *cursor;

    if (full_path == NULL) {
        return (-1);
    }
    path_copy = malloc(strlen(full_path) + 1);
    if (path_copy == NULL) {
        return (-1);
    }
    strcpy(path_copy, full_path);
    cursor = path_copy;
    if (cursor[0] == '/') {
        cursor++;
    }
    while ((cursor = strchr(cursor, '/')) != NULL) {
        *cursor = '\0';
        if (path_copy[0] != '\0' && mkdir(path_copy, 0755) != 0
                && errno != EEXIST) {
            free(path_copy);
            return (-1);
        }
        *cursor = '/';
        cursor++;
    }
    free(path_copy);
    return (0);
}
