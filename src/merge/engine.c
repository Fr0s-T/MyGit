#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "checkout_apply.h"
#include "merge_engine.h"

static int compare_paths(const void *left, const void *right);
static checkout_entry *find_entry_by_path(checkout_entry **entries, int count,
    const char *path);
static int entries_match(const checkout_entry *left,
    const checkout_entry *right);
static int append_unique_path(char ***paths, int *path_count, const char *path);
static int append_conflict_path(char ***conflict_paths, int *conflict_count,
    const char *path);
static int append_merged_entry(checkout_entry ***entries, int *entry_count,
    const checkout_entry *source);

int merge_build_entries(checkout_entry **base_entries, int base_count,
    checkout_entry **current_entries, int current_count,
    checkout_entry **target_entries, int target_count, merge_mode mode,
    checkout_entry ***merged_entries, int *merged_count,
    char ***conflict_paths, int *conflict_count) {
    char **all_paths;
    int all_path_count;
    int status;

    all_paths = NULL;
    all_path_count = 0;
    status = -1;
    if (merged_entries == NULL || merged_count == NULL || conflict_paths == NULL
            || conflict_count == NULL || base_count < 0 || current_count < 0
            || target_count < 0) {
        return (-1);
    }
    *merged_entries = NULL;
    *merged_count = 0;
    *conflict_paths = NULL;
    *conflict_count = 0;
    for (int i = 0; i < base_count; i++) {
        if (base_entries == NULL || base_entries[i] == NULL
                || base_entries[i]->relative_path == NULL
                || append_unique_path(&all_paths, &all_path_count,
                    base_entries[i]->relative_path) != 0) {
            goto cleanup;
        }
    }
    for (int i = 0; i < current_count; i++) {
        if (current_entries == NULL || current_entries[i] == NULL
                || current_entries[i]->relative_path == NULL
                || append_unique_path(&all_paths, &all_path_count,
                    current_entries[i]->relative_path) != 0) {
            goto cleanup;
        }
    }
    for (int i = 0; i < target_count; i++) {
        if (target_entries == NULL || target_entries[i] == NULL
                || target_entries[i]->relative_path == NULL
                || append_unique_path(&all_paths, &all_path_count,
                    target_entries[i]->relative_path) != 0) {
            goto cleanup;
        }
    }
    qsort(all_paths, (size_t)all_path_count, sizeof(char *), compare_paths);
    for (int i = 0; i < all_path_count; i++) {
        checkout_entry *base_entry;
        checkout_entry *current_entry;
        checkout_entry *target_entry;
        const checkout_entry *resolved_entry;
        int is_conflict;

        base_entry = find_entry_by_path(base_entries, base_count, all_paths[i]);
        current_entry = find_entry_by_path(current_entries, current_count,
            all_paths[i]);
        target_entry = find_entry_by_path(target_entries, target_count,
            all_paths[i]);
        resolved_entry = NULL;
        is_conflict = 0;
        if (entries_match(current_entry, target_entry)) {
            resolved_entry = current_entry;
        }
        else if (entries_match(current_entry, base_entry)) {
            resolved_entry = target_entry;
        }
        else if (entries_match(target_entry, base_entry)) {
            resolved_entry = current_entry;
        }
        else if (mode == MERGE_MODE_INCOMING) {
            resolved_entry = target_entry;
        }
        else if (mode == MERGE_MODE_CURRENT) {
            resolved_entry = current_entry;
        }
        else {
            is_conflict = 1;
        }
        if (is_conflict != 0) {
            if (append_conflict_path(conflict_paths, conflict_count,
                    all_paths[i]) != 0) {
                goto cleanup;
            }
            continue;
        }
        if (resolved_entry != NULL && append_merged_entry(merged_entries,
                merged_count, resolved_entry) != 0) {
            goto cleanup;
        }
    }
    if (mode == MERGE_MODE_NONE && *conflict_count > 0) {
        checkout_destroy_entries(*merged_entries, *merged_count);
        *merged_entries = NULL;
        *merged_count = 0;
    }
    status = 0;

cleanup:
    if (status != 0) {
        checkout_destroy_entries(*merged_entries, *merged_count);
        merge_destroy_conflicts(*conflict_paths, *conflict_count);
        *merged_entries = NULL;
        *merged_count = 0;
        *conflict_paths = NULL;
        *conflict_count = 0;
    }
    if (all_paths != NULL) {
        for (int i = 0; i < all_path_count; i++) {
            free(all_paths[i]);
        }
    }
    free(all_paths);
    return (status);
}

void merge_destroy_conflicts(char **conflict_paths, int conflict_count) {
    if (conflict_paths == NULL) {
        return;
    }
    for (int i = 0; i < conflict_count; i++) {
        free(conflict_paths[i]);
    }
    free(conflict_paths);
}

static int compare_paths(const void *left, const void *right) {
    const char *const *left_path;
    const char *const *right_path;

    left_path = left;
    right_path = right;
    return (strcmp(*left_path, *right_path));
}

static checkout_entry *find_entry_by_path(checkout_entry **entries, int count,
    const char *path) {
    if (path == NULL || count <= 0 || entries == NULL) {
        return (NULL);
    }
    for (int i = 0; i < count; i++) {
        if (entries[i] == NULL || entries[i]->relative_path == NULL) {
            continue;
        }
        if (strcmp(entries[i]->relative_path, path) == 0) {
            return (entries[i]);
        }
    }
    return (NULL);
}

static int entries_match(const checkout_entry *left,
    const checkout_entry *right) {
    if (left == NULL && right == NULL) {
        return (1);
    }
    if (left == NULL || right == NULL) {
        return (0);
    }
    return (checkout_entry_compare(left, right) == 0);
}

static int append_unique_path(char ***paths, int *path_count, const char *path) {
    char **new_paths;
    char *path_copy;

    if (paths == NULL || path_count == NULL || path == NULL) {
        return (-1);
    }
    for (int i = 0; i < *path_count; i++) {
        if (strcmp((*paths)[i], path) == 0) {
            return (0);
        }
    }
    path_copy = malloc(strlen(path) + 1);
    if (path_copy == NULL) {
        return (-1);
    }
    strcpy(path_copy, path);
    new_paths = realloc(*paths, (size_t)(*path_count + 1) * sizeof(char *));
    if (new_paths == NULL) {
        free(path_copy);
        return (-1);
    }
    *paths = new_paths;
    (*paths)[*path_count] = path_copy;
    (*path_count)++;
    return (0);
}

static int append_conflict_path(char ***conflict_paths, int *conflict_count,
    const char *path) {
    char **new_conflict_paths;
    char *path_copy;

    if (conflict_paths == NULL || conflict_count == NULL || path == NULL) {
        return (-1);
    }
    path_copy = malloc(strlen(path) + 1);
    if (path_copy == NULL) {
        return (-1);
    }
    strcpy(path_copy, path);
    new_conflict_paths = realloc(*conflict_paths,
        (size_t)(*conflict_count + 1) * sizeof(char *));
    if (new_conflict_paths == NULL) {
        free(path_copy);
        return (-1);
    }
    *conflict_paths = new_conflict_paths;
    (*conflict_paths)[*conflict_count] = path_copy;
    (*conflict_count)++;
    return (0);
}

static int append_merged_entry(checkout_entry ***entries, int *entry_count,
    const checkout_entry *source) {
    checkout_entry **new_entries;
    checkout_entry *new_entry;

    if (entries == NULL || entry_count == NULL || source == NULL
            || source->relative_path == NULL || source->blob_hash == NULL) {
        return (-1);
    }
    new_entry = checkout_entry_create(source->relative_path, source->blob_hash);
    if (new_entry == NULL) {
        return (-1);
    }
    new_entries = realloc(*entries,
        (size_t)(*entry_count + 1) * sizeof(checkout_entry *));
    if (new_entries == NULL) {
        checkout_entry_destroy(new_entry);
        return (-1);
    }
    *entries = new_entries;
    (*entries)[*entry_count] = new_entry;
    (*entry_count)++;
    return (0);
}
