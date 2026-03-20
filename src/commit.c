#include "../include/my_includes.h"

enum e_commit_constants {
    COMMIT_PATH_BUFFER_SIZE = 4096,
};

int input_check(int argc, char **argv);
int build_tree_pass_one(char *index_path, node *root);

int commit(int argc, char **argv) {
    char cwd[PATH_MAX];
    node *root;
    char *temp_path;
    char *index_path;
    int status;

    root = NULL;
    temp_path = NULL;
    index_path = NULL;
    status = -1;
    if (input_check(argc, argv) == -1) {
        return -1;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("\nFailed to get working dir\n");
        return -1;
    }

    root = node_create("root", NULL, NODE_UNKNOWN, NULL);
    if (root == NULL) {
        return -1;
    }

    temp_path = generate_path(cwd, ".mygit");
    if (temp_path == NULL) {
        goto cleanup;
    }
    index_path = generate_path(temp_path, "index");
    if (index_path == NULL) {
        goto cleanup;
    }
    if (build_tree_pass_one(index_path, root) != 0) {
        goto cleanup;
    }
    print_tree(root, 0);
    status = 0;
cleanup:
    free(index_path);
    free(temp_path);
    node_destroy(root);
    return (status);
}

int input_check(int argc, char **argv) {
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
    return (0);
}

int build_tree_pass_one(char *index_path, node *root) {
    FILE *index_file;
    node *current_node;
    node *child_node;
    node *new_node;
    char path[COMMIT_PATH_BUFFER_SIZE];
    char hash[SHA1_HEX_BUFFER_SIZE];
    char *token;

    if (index_path == NULL || root == NULL) {
        return (-1);
    }
    index_file = fopen(index_path, "r");
    if (index_file == NULL) {
        printf("\nCouldn't open file: %s\n", index_path);
        return (-1);
    }
    while (fscanf(index_file, "%4095[^\t]\t%40s", path, hash) == 2) {
        current_node = root;
        /* strtok mutates path, which is fine here because each loop reads a fresh line. */
        token = strtok(path, "/");
        while (token != NULL) {
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
            current_node = child_node;
            token = strtok(NULL, "/");
        }
        /* The leaf keeps the blob hash once the full directory chain is in place. */
        if (current_node->hash != NULL) {
            free(current_node->hash);
            current_node->hash = NULL;
        }
        current_node->hash = malloc(strlen(hash) + 1);
        if (current_node->hash == NULL) {
            fclose(index_file);
            return (-1);
        }
        strcpy(current_node->hash, hash);
    }
    fclose(index_file);
    return (0);
}
