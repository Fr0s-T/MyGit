#ifndef MYGIT_HELPERS_COMMIT_TREE_H
#define MYGIT_HELPERS_COMMIT_TREE_H

#include "../file_data.h"
#include "../node.h"

/*
** Populates an existing root node from an on-disk index file.
**
** Inputs:
** - index_path: path to a `.mygit/index`-style file
** - root: already-created NODE_ROOT node
**
** Output:
** - fills the node tree with file and directory entries
*/
int build_tree_pass_one(char *index_path, node *root);

/*
** Computes directory/root hashes bottom-up.
**
** Inputs:
** - root: populated tree root
**
** Behavior:
** - writes tree objects to `.mygit/objects`
** - sets `root->hash` to the computed root tree hash
*/
int build_tree_second_pass_recursive_ascent(node *root);

/*
** Builds a complete tree from a `.mygit/index`-style file.
**
** Inputs:
** - index_path: path to index-like file entries ("path<TAB>hash")
** - write_objects: non-zero writes tree objects, zero computes hashes only
**
** Output on success:
** - root_out: newly allocated root node
**
** Caller owns root_out and must free it with node_destroy().
*/
int build_tree_from_index_path(const char *index_path, int write_objects,
    node **root_out);

/*
** Builds a complete tree from an in-memory file snapshot.
**
** Inputs:
** - files: array of file_data entries
** - len_files: number of file entries
** - write_objects: non-zero writes tree objects, zero computes hashes only
**
** Output on success:
** - root_out: newly allocated root node
**
** Caller owns root_out and must free it with node_destroy().
*/
int build_tree_from_files(file_data **files, int len_files, int write_objects,
    node **root_out);

#endif
