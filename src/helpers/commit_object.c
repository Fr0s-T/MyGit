#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>

#include "add_creating_blob_and_indexing.h"
#include "hash.h"
#include "helpers/commit_object.h"
#include "helpers/file_io.h"
#include "services.h"

static char *duplicate_string(const char *value);
static int append_parent_hash(commit_object_info *info, const char *parent_hash);
static int write_object_file(const char *object_hash, const char *payload);
static int build_commit_payload(const char *root_hash, const char *branch_name,
    char **parent_hashes, int parent_count, const char *commit_msg,
    char **payload);
static char *extract_branch_name(const char *ref_path);
static int write_commit_to_head(node *root, const char *commit_msg,
    char **parent_hashes, int parent_count, int reject_same_tree,
    char out[SHA1_HEX_BUFFER_SIZE]);
static int parse_prefixed_line(const char *line, const char *prefix, char **out);
static void trim_line_endings(char *value);

int accept_commit(node *root, const char *commit_msg,
    char out[SHA1_HEX_BUFFER_SIZE]) {
    return (write_commit_to_head(root, commit_msg, NULL, 0, 1, out));
}

int accept_commit_with_parents(node *root, const char *commit_msg,
    char **parent_hashes, int parent_count,
    char out[SHA1_HEX_BUFFER_SIZE]) {
    return (write_commit_to_head(root, commit_msg, parent_hashes,
        parent_count, 0, out));
}

int commit_object_read_info(const char *commit_hash, commit_object_info *info) {
    char object_path[PATH_MAX];
    FILE *file;
    char line[PATH_MAX];

    if (commit_hash == NULL || info == NULL) {
        return (-1);
    }
    info->tree_hash = NULL;
    info->branch_name = NULL;
    info->time_value = NULL;
    info->parent_hashes = NULL;
    info->parent_count = 0;
    if (snprintf(object_path, sizeof(object_path), ".mygit/objects/%s",
            commit_hash) >= (int)sizeof(object_path)) {
        return (-1);
    }
    file = fopen(object_path, "r");
    if (file == NULL) {
        return (-1);
    }
    if (fgets(line, sizeof(line), file) == NULL
            || parse_prefixed_line(line, "tree ", &info->tree_hash) != 0) {
        fclose(file);
        commit_object_destroy_info(info);
        return (-1);
    }
    if (fgets(line, sizeof(line), file) == NULL
            || parse_prefixed_line(line, "branch ", &info->branch_name) != 0) {
        fclose(file);
        commit_object_destroy_info(info);
        return (-1);
    }
    if (fgets(line, sizeof(line), file) == NULL
            || parse_prefixed_line(line, "time ", &info->time_value) != 0) {
        fclose(file);
        commit_object_destroy_info(info);
        return (-1);
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char *parent_hash;

        trim_line_endings(line);
        if (line[0] == '\0') {
            break;
        }
        parent_hash = NULL;
        if (parse_prefixed_line(line, "parent ", &parent_hash) != 0) {
            fclose(file);
            commit_object_destroy_info(info);
            return (-1);
        }
        if (strcmp(parent_hash, "NULL") != 0
                && append_parent_hash(info, parent_hash) != 0) {
            free(parent_hash);
            fclose(file);
            commit_object_destroy_info(info);
            return (-1);
        }
        free(parent_hash);
    }
    fclose(file);
    return (0);
}

void commit_object_destroy_info(commit_object_info *info) {
    if (info == NULL) {
        return;
    }
    free(info->tree_hash);
    free(info->branch_name);
    free(info->time_value);
    if (info->parent_hashes != NULL) {
        for (int i = 0; i < info->parent_count; i++) {
            free(info->parent_hashes[i]);
        }
    }
    free(info->parent_hashes);
    info->tree_hash = NULL;
    info->branch_name = NULL;
    info->time_value = NULL;
    info->parent_hashes = NULL;
    info->parent_count = 0;
}

int read_commit_tree_hash(const char *commit_hash, char **tree_hash) {
    commit_object_info info;
    int status;

    if (commit_hash == NULL || tree_hash == NULL) {
        return (-1);
    }
    *tree_hash = NULL;
    status = commit_object_read_info(commit_hash, &info);
    if (status != 0) {
        return (-1);
    }
    *tree_hash = info.tree_hash;
    info.tree_hash = NULL;
    commit_object_destroy_info(&info);
    return (*tree_hash == NULL ? -1 : 0);
}

static int write_commit_to_head(node *root, const char *commit_msg,
    char **parent_hashes, int parent_count, int reject_same_tree,
    char out[SHA1_HEX_BUFFER_SIZE]) {
    char *head_ref_path;
    char *current_commit_hash;
    char *previous_tree_hash;
    char *branch_name;
    char *payload;
    char *new_commit_hash;
    char *default_parent_hashes[1];
    char **effective_parent_hashes;
    int effective_parent_count;
    int status;

    head_ref_path = NULL;
    current_commit_hash = NULL;
    previous_tree_hash = NULL;
    branch_name = NULL;
    payload = NULL;
    new_commit_hash = NULL;
    effective_parent_hashes = parent_hashes;
    effective_parent_count = parent_count;
    status = -1;
    if (root == NULL || root->hash == NULL || commit_msg == NULL || out == NULL) {
        return (-1);
    }
    if (parent_count < 0 || (parent_count > 0 && parent_hashes == NULL)) {
        return (-1);
    }
    if (file_io_read_first_line(".mygit/HEAD", &head_ref_path) == -1) {
        goto cleanup;
    }
    if (head_ref_path[0] == '\0') {
        goto cleanup;
    }
    if (file_io_read_first_line(head_ref_path, &current_commit_hash) == -1) {
        goto cleanup;
    }
    if (effective_parent_hashes == NULL && current_commit_hash[0] != '\0') {
        default_parent_hashes[0] = current_commit_hash;
        effective_parent_hashes = default_parent_hashes;
        effective_parent_count = 1;
    }
    if (reject_same_tree != 0 && current_commit_hash[0] != '\0') {
        if (read_commit_tree_hash(current_commit_hash, &previous_tree_hash) == -1) {
            goto cleanup;
        }
        if (strcmp(previous_tree_hash, root->hash) == 0) {
            status = COMMIT_NOTHING_TO_DO;
            goto cleanup;
        }
    }
    branch_name = extract_branch_name(head_ref_path);
    if (branch_name == NULL) {
        goto cleanup;
    }
    if (build_commit_payload(root->hash, branch_name, effective_parent_hashes,
            effective_parent_count, commit_msg, &payload) == -1) {
        goto cleanup;
    }
    new_commit_hash = sha1(payload);
    if (new_commit_hash == NULL) {
        goto cleanup;
    }
    if (write_object_file(new_commit_hash, payload) == -1) {
        goto cleanup;
    }
    if (file_io_write_text(head_ref_path, new_commit_hash) == -1) {
        if (current_commit_hash != NULL
                && file_io_write_text(head_ref_path, current_commit_hash) == -1) {
            printf("[commit] failed to restore branch ref\n");
        }
        goto cleanup;
    }
    strcpy(out, new_commit_hash);
    status = 0;

cleanup:
    free(new_commit_hash);
    free(payload);
    free(branch_name);
    free(previous_tree_hash);
    free(current_commit_hash);
    free(head_ref_path);
    return (status);
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

static int append_parent_hash(commit_object_info *info, const char *parent_hash) {
    char **new_parent_hashes;
    char *parent_copy;

    if (info == NULL || parent_hash == NULL) {
        return (-1);
    }
    parent_copy = duplicate_string(parent_hash);
    if (parent_copy == NULL) {
        return (-1);
    }
    new_parent_hashes = realloc(info->parent_hashes,
        (size_t)(info->parent_count + 1) * sizeof(char *));
    if (new_parent_hashes == NULL) {
        free(parent_copy);
        return (-1);
    }
    info->parent_hashes = new_parent_hashes;
    info->parent_hashes[info->parent_count] = parent_copy;
    info->parent_count++;
    return (0);
}

static int write_object_file(const char *object_hash, const char *payload) {
    char cwd[PATH_MAX];
    char *git_obj_path;
    char *object_path;
    int status;

    if (object_hash == NULL || payload == NULL) {
        return (-1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return (-1);
    }
    git_obj_path = create_git_obj_dir(cwd);
    if (git_obj_path == NULL) {
        return (-1);
    }
    object_path = generate_path(git_obj_path, object_hash);
    free(git_obj_path);
    if (object_path == NULL) {
        return (-1);
    }
    status = file_io_write_text(object_path, payload);
    if (status == -1) {
        remove(object_path);
    }
    free(object_path);
    return (status);
}

static int build_commit_payload(const char *root_hash, const char *branch_name,
    char **parent_hashes, int parent_count, const char *commit_msg,
    char **payload) {
    time_t now;
    char time_buffer[32];
    int needed;
    int written;

    if (root_hash == NULL || branch_name == NULL || commit_msg == NULL
            || payload == NULL || parent_count < 0
            || (parent_count > 0 && parent_hashes == NULL)) {
        return (-1);
    }
    now = time(NULL);
    if (now == (time_t)-1) {
        return (-1);
    }
    snprintf(time_buffer, sizeof(time_buffer), "%ld", (long)now);
    needed = snprintf(NULL, 0, "tree %s\nbranch %s\ntime %s\n",
        root_hash, branch_name, time_buffer);
    if (needed < 0) {
        return (-1);
    }
    if (parent_count == 0) {
        needed += snprintf(NULL, 0, "parent NULL\n");
    }
    else {
        for (int i = 0; i < parent_count; i++) {
            if (parent_hashes[i] == NULL || parent_hashes[i][0] == '\0') {
                return (-1);
            }
            needed += snprintf(NULL, 0, "parent %s\n", parent_hashes[i]);
        }
    }
    needed += snprintf(NULL, 0, "\n%s", commit_msg);
    if (needed < 0) {
        return (-1);
    }
    *payload = malloc((size_t)needed + 1);
    if (*payload == NULL) {
        return (-1);
    }
    written = snprintf(*payload, (size_t)needed + 1, "tree %s\nbranch %s\ntime %s\n",
        root_hash, branch_name, time_buffer);
    if (written < 0 || written > needed) {
        free(*payload);
        *payload = NULL;
        return (-1);
    }
    if (parent_count == 0) {
        written += snprintf(*payload + written, (size_t)needed + 1 - (size_t)written,
            "parent NULL\n");
    }
    else {
        for (int i = 0; i < parent_count; i++) {
            written += snprintf(*payload + written,
                (size_t)needed + 1 - (size_t)written,
                "parent %s\n", parent_hashes[i]);
        }
    }
    snprintf(*payload + written, (size_t)needed + 1 - (size_t)written,
        "\n%s", commit_msg);
    return (0);
}

static char *extract_branch_name(const char *ref_path) {
    const char *last_slash;

    if (ref_path == NULL) {
        return (NULL);
    }
    last_slash = strrchr(ref_path, '/');
    if (last_slash == NULL || *(last_slash + 1) == '\0') {
        return (duplicate_string(ref_path));
    }
    return (duplicate_string(last_slash + 1));
}

static int parse_prefixed_line(const char *line, const char *prefix, char **out) {
    const char *value;

    if (line == NULL || prefix == NULL || out == NULL) {
        return (-1);
    }
    if (strncmp(line, prefix, strlen(prefix)) != 0) {
        return (-1);
    }
    value = line + strlen(prefix);
    *out = duplicate_string(value);
    if (*out == NULL) {
        return (-1);
    }
    trim_line_endings(*out);
    return (0);
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
