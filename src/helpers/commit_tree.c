#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "add_creating_blob_and_indexing.h"
#include "hash.h"
#include "helpers/file_io.h"
#include "helpers/commit_tree.h"
#include "node.h"
#include "services.h"

enum e_commit_tree_constants {
    COMMIT_PATH_BUFFER_SIZE = 4096,
};

static void trim_line_endings(char *value);
static void trim_leading_line_endings(char *value);
static int add_index_entry_to_tree(node *root, const char *path_value,
    const char *hash_value);
static int finalize_tree_hashes(node *root, int write_objects);
static int write_tree_object(const char *tree_hash, const char *payload);
static int write_object_file(const char *object_hash, const char *payload);

int build_tree_pass_one(char *index_path, node *root) {
    FILE *index_file;
    char path[COMMIT_PATH_BUFFER_SIZE];
    char hash[SHA1_HEX_BUFFER_SIZE];

    if (index_path == NULL || root == NULL) {
        return (-1);
    }
    index_file = fopen(index_path, "r");
    if (index_file == NULL) {
        return (-1);
    }
    while (fscanf(index_file, " %4095[^\t]\t%40s", path, hash) == 2) {
        trim_leading_line_endings(path);
        trim_leading_line_endings(hash);
        trim_line_endings(path);
        trim_line_endings(hash);
        if (add_index_entry_to_tree(root, path, hash) != 0) {
            fclose(index_file);
            return (-1);
        }
    }
    fclose(index_file);
    return (0);
}

int build_tree_second_pass_recursive_ascent(node *root) {
    return (finalize_tree_hashes(root, 1));
}

int build_tree_from_index_path(const char *index_path, int write_objects,
    node **root_out) {
    node *root;

    if (index_path == NULL || root_out == NULL) {
        return (-1);
    }
    *root_out = NULL;
    root = node_create("root", NULL, NODE_ROOT, NULL);
    if (root == NULL) {
        return (-1);
    }
    if (build_tree_pass_one((char *)index_path, root) != 0) {
        node_destroy(root);
        return (-1);
    }
    if (finalize_tree_hashes(root, write_objects) != 0) {
        node_destroy(root);
        return (-1);
    }
    *root_out = root;
    return (0);
}

int build_tree_from_files(file_data **files, int len_files, int write_objects,
    node **root_out) {
    node *root;

    if (files == NULL || len_files < 0 || root_out == NULL) {
        return (-1);
    }
    *root_out = NULL;
    root = node_create("root", NULL, NODE_ROOT, NULL);
    if (root == NULL) {
        return (-1);
    }
    for (int i = 0; i < len_files; i++) {
        if (files[i] == NULL || files[i]->path == NULL || files[i]->hash == NULL) {
            node_destroy(root);
            return (-1);
        }
        if (add_index_entry_to_tree(root, files[i]->path, files[i]->hash) != 0) {
            node_destroy(root);
            return (-1);
        }
    }
    if (finalize_tree_hashes(root, write_objects) != 0) {
        node_destroy(root);
        return (-1);
    }
    *root_out = root;
    return (0);
}

static int add_index_entry_to_tree(node *root, const char *path_value,
    const char *hash_value) {
    char *string;
    char *token;
    char *next_token;
    node *current_node;
    node *child_node;
    node *new_node;

    if (root == NULL || path_value == NULL || hash_value == NULL) {
        return (-1);
    }
    string = malloc(strlen(path_value) + 1);
    if (string == NULL) {
        return (-1);
    }
    strcpy(string, path_value);
    current_node = root;
    token = strtok(string, "/");
    while (token != NULL) {
        next_token = strtok(NULL, "/");
        child_node = find_child(current_node, token);
        if (child_node == NULL) {
            new_node = node_create(token, NULL, NODE_UNKNOWN, current_node);
            if (new_node == NULL) {
                free(string);
                return (-1);
            }
            if (node_add_child(current_node, new_node) == -1) {
                node_destroy(new_node);
                free(string);
                return (-1);
            }
            child_node = new_node;
        }
        if (next_token != NULL && child_node->type == NODE_UNKNOWN) {
            child_node->type = NODE_DIR;
        }
        current_node = child_node;
        token = next_token;
    }
    if (current_node->hash != NULL) {
        free(current_node->hash);
        current_node->hash = NULL;
    }
    current_node->hash = malloc(strlen(hash_value) + 1);
    if (current_node->hash == NULL) {
        free(string);
        return (-1);
    }
    current_node->type = NODE_FILE;
    strcpy(current_node->hash, hash_value);
    free(string);
    return (0);
}

static int finalize_tree_hashes(node *root, int write_objects) {
    char *string;
    char *type_string;
    char *new_hash;

    if (root == NULL) {
        return (-1);
    }
    if (root->type == NODE_FILE) {
        return (0);
    }
    if (root->type == NODE_ROOT || root->type == NODE_DIR) {
        if (root->hash == NULL) {
            for (int i = 0; i < root->children_count; i++) {
                if (finalize_tree_hashes(root->children[i], write_objects) == -1) {
                    return (-1);
                }
            }
            string = malloc(((PATH_MAX + 50) * root->children_count) + 1);
            if (string == NULL) {
                return (-1);
            }
            string[0] = '\0';
            for (int i = 0; i < root->children_count; i++) {
                if (root->children[i]->type == NODE_FILE) {
                    type_string = "blob";
                }
                else {
                    type_string = "tree";
                }
                strcat(string, type_string);
                strcat(string, " ");
                strcat(string, root->children[i]->name);
                strcat(string, " ");
                strcat(string, root->children[i]->hash);
                strcat(string, "\n");
            }
            new_hash = sha1(string);
            if (new_hash == NULL) {
                free(string);
                return (-1);
            }
            if (write_objects != 0 && write_tree_object(new_hash, string) == -1) {
                free(new_hash);
                free(string);
                return (-1);
            }
            root->hash = new_hash;
            free(string);
        }
        return (0);
    }
    return (-1);
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

static void trim_leading_line_endings(char *value) {
    size_t start;
    size_t len;

    if (value == NULL) {
        return ;
    }
    start = 0;
    while (value[start] == '\n' || value[start] == '\r') {
        start++;
    }
    if (start == 0) {
        return ;
    }
    len = strlen(value + start);
    memmove(value, value + start, len + 1);
}

static int write_tree_object(const char *tree_hash, const char *payload) {
    return (write_object_file(tree_hash, payload));
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
