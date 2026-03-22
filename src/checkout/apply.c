#include <errno.h>

#include "checkout_apply.h"
#include "my_includes.h"

static int checkout_entry_is_surviving(checkout_entry *entry,
    checkout_entry **surviving_entries, int surviving_count);
static int checkout_path_is_tracked(const char *relative_path,
    checkout_entry **entries, int count);
static void remove_empty_parent_directories(const char *relative_path);
static int ensure_parent_directories(const char *full_path);

void checkout_destroy_entries(checkout_entry **entries, int count) {
    if (entries == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        checkout_entry_destroy(entries[i]);
    }
    free(entries);
}

void checkout_print_entries(const char *label, checkout_entry **entries,
    int count) {
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

int checkout_build_surviving_entries(checkout_entry ***surviving_entries,
    checkout_entry **target_entries, checkout_entry **current_entries,
    int target_count, int current_count, int *surviving_count) {
    checkout_entry **new_entries;
    checkout_entry *new_entry;

    if (surviving_entries == NULL || surviving_count == NULL
            || target_count < 0 || current_count < 0) {
        return (-1);
    }
    *surviving_entries = NULL;
    *surviving_count = 0;
    if ((target_count > 0 && target_entries == NULL)
            || (current_count > 0 && current_entries == NULL)) {
        return (-1);
    }
    for (int i = 0; i < target_count; i++) {
        for (int j = 0; j < current_count; j++) {
            if (checkout_entry_compare(target_entries[i], current_entries[j]) != 0) {
                continue;
            }
            new_entries = realloc(*surviving_entries,
                (size_t)(*surviving_count + 1) * sizeof(checkout_entry *));
            if (new_entries == NULL) {
                checkout_destroy_entries(*surviving_entries, *surviving_count);
                *surviving_entries = NULL;
                *surviving_count = 0;
                return (-1);
            }
            *surviving_entries = new_entries;
            new_entry = checkout_entry_create(target_entries[i]->relative_path,
                target_entries[i]->blob_hash);
            if (new_entry == NULL) {
                checkout_destroy_entries(*surviving_entries, *surviving_count);
                *surviving_entries = NULL;
                *surviving_count = 0;
                return (-1);
            }
            (*surviving_entries)[*surviving_count] = new_entry;
            (*surviving_count)++;
            break;
        }
    }
    return (0);
}

int checkout_purge_non_surviving_current_entries(
    checkout_entry **current_entries, int current_count,
    checkout_entry **surviving_entries, int surviving_count) {
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

int checkout_materialize_target_entries(checkout_entry **target_entries,
    int target_count) {
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

int checkout_rewrite_index_from_entries(checkout_entry **entries, int count) {
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

int checkout_validate_entries_available(checkout_entry **entries, int count) {
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

int checkout_restore_state(checkout_entry **current_entries, int current_count,
    checkout_entry **target_entries, int target_count,
    checkout_entry **surviving_entries, int surviving_count) {
    if (checkout_purge_non_surviving_current_entries(target_entries,
            target_count, surviving_entries, surviving_count) != 0) {
        return (-1);
    }
    if (checkout_materialize_target_entries(current_entries, current_count) != 0) {
        return (-1);
    }
    if (checkout_rewrite_index_from_entries(current_entries, current_count) != 0) {
        return (-1);
    }
    return (0);
}

int checkout_target_conflicts_with_untracked(checkout_entry **target_entries,
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

static int checkout_path_is_tracked(const char *relative_path,
    checkout_entry **entries, int count) {
    if (relative_path == NULL || count < 0) {
        return (0);
    }
    if (count == 0 || entries == NULL) {
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
