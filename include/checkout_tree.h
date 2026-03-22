#ifndef MYGIT_CHECKOUT_TREE_H
#define MYGIT_CHECKOUT_TREE_H

#include "checkout_entry.h"
#include "node.h"

/*
** Rebuilds a target tree from a tree object path and collects tracked file
** entries while descending through the object hierarchy.
**
** Inputs:
** - root: already-created NODE_ROOT node
** - root_path: path to the root tree object under .mygit/objects
**
** Outputs on success:
** - target_entries: heap array of checkout_entry pointers
** - entry_count: number of collected file entries
**
** Caller owns target_entries and each entry.
*/
int checkout_generate_tree(node *root, const char *root_path,
    checkout_entry ***target_entries, int *entry_count);

#endif
