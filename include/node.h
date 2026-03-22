#ifndef MY_GIT_NODE
#define MY_GIT_NODE

#include <limits.h>

typedef enum e_node_type {
    NODE_ROOT,
    NODE_UNKNOWN,
    NODE_FILE,
    NODE_DIR
} node_type;

typedef struct s_node {
    /* Display / path segment name for this tree node, owned by the struct. */
    char *name;
    /* Object hash for the node, owned by the struct. May be NULL until built. */
    char *hash;
    /* Whether this node is root, directory, file, or still unresolved. */
    node_type type;
    /* Borrowed back-pointer to the parent node in the same tree. */
    struct s_node *parent;
    /* Number of valid entries in children. */
    int children_count;
    /* Allocated capacity of children. */
    int children_capacity;
    /* Heap array of owned child pointers. */
    struct s_node **children;
} node;

/*
** Creates a node by copying name/hash strings.
**
** Ownership:
** - borrows name, hash, and parent
** - caller owns the returned node until it is attached to a parent or
**   destroyed with node_destroy()
*/
node *node_create(const char *name, const char *hash,
    node_type type, node *parent);

/*
** Recursively frees a node tree and all owned descendants.
*/
void node_destroy(node *root);

/*
** Finds a direct child of current_node by name.
**
** Returns:
** - borrowed pointer to an existing child
** - NULL if not found or on invalid input
*/
node *find_child(node *current_node, char *child_name);

/*
** Pretty-prints the node tree for debugging.
**
** Ownership:
** - borrows root
*/
void print_tree(node *root, int depth);

/*
** Appends child to parent's child array.
**
** Ownership:
** - on success, parent takes ownership of child
** - on failure, caller retains ownership of child
*/
int node_add_child(node *parent, node *child);

/*
** Builds the repo-relative path for a node by walking up through its parents.
**
** Examples:
** - root child "src" -> "src"
** - nested file under src -> "src/main.c"
**
** Returns:
**  0  on success
** -1  on allocation or invalid-input failure
**
** Ownership:
** - allocates *node_path on success
** - caller must free(*node_path)
*/
int get_nodes_path(node *child, char **node_path);

#endif
