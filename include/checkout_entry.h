#ifndef MYGIT_CHECKOUT_ENTRY_H
#define MYGIT_CHECKOUT_ENTRY_H

typedef struct s_checkout_entry {
    char *relative_path;
    char *blob_hash;
    char *object_path;
} checkout_entry;

/*
** Creates one tracked checkout entry.
**
** Inputs:
** - relative_path: repo-relative file path such as "src/main.c"
** - blob_hash: object hash for the file content
**
** Output on success:
** - returns a heap-allocated checkout_entry
** - object_path is also prebuilt as ".mygit/objects/<blob_hash>"
**
** Caller owns the returned entry and must destroy it with
** checkout_entry_destroy().
*/
checkout_entry *checkout_entry_create(const char *relative_path,
    const char *blob_hash);

/*
** Replaces the blob hash on an existing entry and rebuilds object_path.
**
** Returns:
**  0  on success
** -1  on invalid input or allocation failure
*/
int checkout_entry_set_blob_hash(checkout_entry *entry, const char *blob_hash);

/*
** Compares two checkout entries by tracked file identity and content.
**
** Returns:
**  0  if both entries have the same relative_path and blob_hash
** -1  otherwise
*/
int checkout_entry_compare(const checkout_entry *left,
    const checkout_entry *right);

/*
** Frees one checkout entry and all owned strings.
*/
void checkout_entry_destroy(checkout_entry *entry);

#endif
