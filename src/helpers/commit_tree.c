#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "../../include/add_creating_blob_and_indexing.h"
#include "../../include/hash.h"
#include "../../include/helpers/commit_tree.h"
#include "../../include/node.h"
#include "../../include/services.h"

enum e_commit_tree_constants {
    COMMIT_PATH_BUFFER_SIZE = 4096,
};

static void trim_line_endings(char *value);
static void trim_leading_line_endings(char *value);
static int write_tree_object(const char *tree_hash, const char *payload);
static int write_object_file(const char *object_hash, const char *payload);
static int write_content_to_file(const char *path, const char *content);

int build_tree_pass_one(char *index_path, node *root) {
    FILE *index_file;
    node *current_node;
    node *child_node;
    node *new_node;
    char path[COMMIT_PATH_BUFFER_SIZE];
    char hash[SHA1_HEX_BUFFER_SIZE];
    char *token;
    char *next_token;

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
        current_node = root;
        token = strtok(path, "/");
        while (token != NULL) {
            next_token = strtok(NULL, "/");
            child_node = find_child(current_node, token);
            if (child_node == NULL) {
                new_node = node_create(token, NULL, NODE_UNKNOWN, current_node);
                if (new_node == NULL) {
                    fclose(index_file);
                    return (-1);
                }
                if (node_add_child(current_node, new_node) == -1) {
                    node_destroy(new_node);
                    fclose(index_file);
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
        current_node->hash = malloc(strlen(hash) + 1);
        if (current_node->hash == NULL) {
            fclose(index_file);
            return (-1);
        }
        current_node->type = NODE_FILE;
        strcpy(current_node->hash, hash);
    }
    fclose(index_file);
    return (0);
}

int build_tree_second_pass_recursive_ascent(node *root) {
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
                if (build_tree_second_pass_recursive_ascent(root->children[i]) == -1) {
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
            if (write_tree_object(new_hash, string) == -1) {
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
    status = write_content_to_file(object_path, payload);
    if (status == -1) {
        remove(object_path);
    }
    free(object_path);
    return (status);
}

static int write_content_to_file(const char *path, const char *content) {
    FILE *file;
    size_t content_len;

    if (path == NULL || content == NULL) {
        return (-1);
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return (-1);
    }
    content_len = strlen(content);
    if (fwrite(content, 1, content_len, file) != content_len) {
        fclose(file);
        return (-1);
    }
    fclose(file);
    return (0);
}
