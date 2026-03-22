#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "branch.h"
#include "checkout_apply.h"
#include "checkout_prepare.h"
#include "checkout_tree.h"
#include "helpers/commit_object.h"
#include "helpers/file_io.h"
#include "merge_prepare.h"
#include "node.h"

typedef struct s_merge_history_entry {
    char *hash;
    int depth;
} merge_history_entry;

static char *duplicate_string(const char *value);
static void trim_line_endings(char *value);
static int find_history_entry(merge_history_entry *entries, int count,
    const char *hash);
static int append_history_entry(merge_history_entry **entries, int *count,
    const char *hash, int depth);
static int collect_commit_depths(const char *commit_hash, int depth,
    merge_history_entry **entries, int *count);
static void destroy_history_entries(merge_history_entry *entries, int count);

int merge_input_check(int argc, char **argv, merge_mode *mode,
    char **branch_name) {
    if (mode == NULL || branch_name == NULL) {
        return (-1);
    }
    if (argc == 3 && strcmp(argv[1], "merge") == 0) {
        *mode = MERGE_MODE_NONE;
        *branch_name = argv[2];
        return (0);
    }
    if (argc == 4 && strcmp(argv[1], "merge") == 0) {
        if (strcmp(argv[2], "-i") == 0) {
            *mode = MERGE_MODE_INCOMING;
            *branch_name = argv[3];
            return (0);
        }
        if (strcmp(argv[2], "-c") == 0) {
            *mode = MERGE_MODE_CURRENT;
            *branch_name = argv[3];
            return (0);
        }
    }
    printf("[merge] usage: mygit merge branch_name\n");
    printf("[merge]        mygit merge -i branch_name\n");
    printf("[merge]        mygit merge -c branch_name\n");
    return (-1);
}

int merge_read_current_state(char **current_ref_path, char **current_commit_hash) {
    if (current_ref_path == NULL || current_commit_hash == NULL) {
        return (-1);
    }
    *current_ref_path = NULL;
    *current_commit_hash = NULL;
    if (file_io_read_first_line(".mygit/HEAD", current_ref_path) != 0) {
        return (-1);
    }
    if (*current_ref_path == NULL || (*current_ref_path)[0] == '\0') {
        free(*current_ref_path);
        *current_ref_path = NULL;
        return (-1);
    }
    if (file_io_read_first_line(*current_ref_path, current_commit_hash) != 0) {
        free(*current_ref_path);
        *current_ref_path = NULL;
        return (-1);
    }
    return (0);
}

int merge_prompt_use_fast_forward(const char *branch_name, merge_mode mode) {
    char answer[32];
    const char *mode_name;

    if (branch_name == NULL) {
        return (-1);
    }
    if (mode == MERGE_MODE_INCOMING) {
        mode_name = "incoming";
    }
    else if (mode == MERGE_MODE_CURRENT) {
        mode_name = "current";
    }
    else {
        return (1);
    }
    while (1) {
        printf("[merge] fast-forward is possible for '%s'\n", branch_name);
        printf("[merge] use fast-forward instead of %s merge? [Y/n] ",
            mode_name);
        if (fgets(answer, sizeof(answer), stdin) == NULL) {
            if (feof(stdin)) {
                clearerr(stdin);
                return (1);
            }
            return (-1);
        }
        trim_line_endings(answer);
        if (answer[0] == '\0' || strcmp(answer, "y") == 0
                || strcmp(answer, "Y") == 0
                || strcmp(answer, "yes") == 0
                || strcmp(answer, "YES") == 0
                || strcmp(answer, "Yes") == 0) {
            return (1);
        }
        if (strcmp(answer, "n") == 0 || strcmp(answer, "N") == 0
                || strcmp(answer, "no") == 0
                || strcmp(answer, "NO") == 0
                || strcmp(answer, "No") == 0) {
            return (0);
        }
        printf("[merge] answer with 'y' or 'n'\n");
    }
}

int merge_collect_entries_from_commit(const char *commit_hash,
    checkout_entry ***entries, int *entry_count) {
    char *root_hash;
    char *root_path;
    node *root;
    int status;

    root_hash = NULL;
    root_path = NULL;
    root = NULL;
    status = -1;
    if (commit_hash == NULL || entries == NULL || entry_count == NULL) {
        return (-1);
    }
    *entries = NULL;
    *entry_count = 0;
    if (commit_hash[0] == '\0') {
        return (0);
    }
    if (checkout_read_target_root(commit_hash, &root_hash, &root_path) != 0) {
        goto cleanup;
    }
    root = node_create("", root_hash, NODE_ROOT, NULL);
    if (root == NULL) {
        goto cleanup;
    }
    if (root_path != NULL && checkout_generate_tree(root, root_path, entries,
            entry_count) != 0) {
        checkout_destroy_entries(*entries, *entry_count);
        *entries = NULL;
        *entry_count = 0;
        goto cleanup;
    }
    status = 0;

cleanup:
    node_destroy(root);
    free(root_path);
    free(root_hash);
    return (status);
}

int merge_commit_is_ancestor(const char *ancestor_hash, const char *commit_hash) {
    merge_history_entry *entries;
    int count;
    int status;

    entries = NULL;
    count = 0;
    status = -1;
    if (ancestor_hash == NULL || commit_hash == NULL) {
        return (-1);
    }
    if (ancestor_hash[0] == '\0') {
        return (commit_hash[0] == '\0');
    }
    if (commit_hash[0] == '\0') {
        return (0);
    }
    if (collect_commit_depths(commit_hash, 0, &entries, &count) != 0) {
        goto cleanup;
    }
    status = (find_history_entry(entries, count, ancestor_hash) >= 0);

cleanup:
    destroy_history_entries(entries, count);
    return (status);
}

int merge_find_merge_base(const char *left_hash, const char *right_hash,
    char **merge_base_hash) {
    merge_history_entry *left_entries;
    merge_history_entry *right_entries;
    int left_count;
    int right_count;
    int best_sum;
    int best_right_depth;
    const char *best_hash;

    left_entries = NULL;
    right_entries = NULL;
    left_count = 0;
    right_count = 0;
    best_sum = -1;
    best_right_depth = -1;
    best_hash = NULL;
    if (left_hash == NULL || right_hash == NULL || merge_base_hash == NULL) {
        return (-1);
    }
    *merge_base_hash = NULL;
    if (left_hash[0] == '\0' || right_hash[0] == '\0') {
        *merge_base_hash = duplicate_string("");
        return (*merge_base_hash == NULL ? -1 : 0);
    }
    if (collect_commit_depths(left_hash, 0, &left_entries, &left_count) != 0) {
        goto cleanup;
    }
    if (collect_commit_depths(right_hash, 0, &right_entries, &right_count) != 0) {
        goto cleanup;
    }
    for (int i = 0; i < left_count; i++) {
        int right_index;
        int depth_sum;

        right_index = find_history_entry(right_entries, right_count,
            left_entries[i].hash);
        if (right_index < 0) {
            continue;
        }
        depth_sum = left_entries[i].depth + right_entries[right_index].depth;
        if (best_hash == NULL || depth_sum < best_sum
                || (depth_sum == best_sum
                    && right_entries[right_index].depth < best_right_depth)) {
            best_hash = left_entries[i].hash;
            best_sum = depth_sum;
            best_right_depth = right_entries[right_index].depth;
        }
    }
    if (best_hash == NULL) {
        *merge_base_hash = duplicate_string("");
    }
    else {
        *merge_base_hash = duplicate_string(best_hash);
    }

cleanup:
    destroy_history_entries(left_entries, left_count);
    destroy_history_entries(right_entries, right_count);
    return (*merge_base_hash == NULL ? -1 : 0);
}

char *merge_build_default_message(const char *branch_name) {
    char *message;
    int needed;

    if (branch_name == NULL) {
        return (NULL);
    }
    needed = snprintf(NULL, 0, "Merge branch '%s'", branch_name);
    if (needed < 0) {
        return (NULL);
    }
    message = malloc((size_t)needed + 1);
    if (message == NULL) {
        return (NULL);
    }
    snprintf(message, (size_t)needed + 1, "Merge branch '%s'", branch_name);
    return (message);
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

static void trim_line_endings(char *value) {
    size_t len;

    if (value == NULL) {
        return ;
    }
    len = strlen(value);
    while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) {
        value[len - 1] = '\0';
        len--;
    }
}

static int find_history_entry(merge_history_entry *entries, int count,
    const char *hash) {
    if (entries == NULL || hash == NULL) {
        return (-1);
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].hash, hash) == 0) {
            return (i);
        }
    }
    return (-1);
}

static int append_history_entry(merge_history_entry **entries, int *count,
    const char *hash, int depth) {
    merge_history_entry *new_entries;
    char *hash_copy;

    if (entries == NULL || count == NULL || hash == NULL) {
        return (-1);
    }
    hash_copy = duplicate_string(hash);
    if (hash_copy == NULL) {
        return (-1);
    }
    new_entries = realloc(*entries,
        (size_t)(*count + 1) * sizeof(merge_history_entry));
    if (new_entries == NULL) {
        free(hash_copy);
        return (-1);
    }
    *entries = new_entries;
    (*entries)[*count].hash = hash_copy;
    (*entries)[*count].depth = depth;
    (*count)++;
    return (0);
}

static int collect_commit_depths(const char *commit_hash, int depth,
    merge_history_entry **entries, int *count) {
    commit_object_info info;
    int existing_index;

    if (commit_hash == NULL || entries == NULL || count == NULL || depth < 0) {
        return (-1);
    }
    if (commit_hash[0] == '\0' || strcmp(commit_hash, "NULL") == 0) {
        return (0);
    }
    existing_index = find_history_entry(*entries, *count, commit_hash);
    if (existing_index >= 0) {
        if (depth >= (*entries)[existing_index].depth) {
            return (0);
        }
        (*entries)[existing_index].depth = depth;
    }
    else if (append_history_entry(entries, count, commit_hash, depth) != 0) {
        return (-1);
    }
    if (commit_object_read_info(commit_hash, &info) != 0) {
        return (-1);
    }
    for (int i = 0; i < info.parent_count; i++) {
        if (collect_commit_depths(info.parent_hashes[i], depth + 1, entries,
                count) != 0) {
            commit_object_destroy_info(&info);
            return (-1);
        }
    }
    commit_object_destroy_info(&info);
    return (0);
}

static void destroy_history_entries(merge_history_entry *entries, int count) {
    if (entries == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        free(entries[i].hash);
    }
    free(entries);
}
