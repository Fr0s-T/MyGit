#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "checkout_entry.h"

static char *checkout_entry_strdup(const char *value);
static char *checkout_entry_build_object_path(const char *blob_hash);

checkout_entry *checkout_entry_create(const char *relative_path,
    const char *blob_hash) {
    checkout_entry *entry;

    if (relative_path == NULL || blob_hash == NULL) {
        return (NULL);
    }
    entry = malloc(sizeof(checkout_entry));
    if (entry == NULL) {
        return (NULL);
    }
    entry->relative_path = NULL;
    entry->blob_hash = NULL;
    entry->object_path = NULL;
    entry->relative_path = checkout_entry_strdup(relative_path);
    if (entry->relative_path == NULL) {
        checkout_entry_destroy(entry);
        return (NULL);
    }
    if (checkout_entry_set_blob_hash(entry, blob_hash) != 0) {
        checkout_entry_destroy(entry);
        return (NULL);
    }
    return (entry);
}

int checkout_entry_set_blob_hash(checkout_entry *entry, const char *blob_hash) {
    char *new_hash;
    char *new_object_path;

    if (entry == NULL || blob_hash == NULL) {
        return (-1);
    }
    new_hash = checkout_entry_strdup(blob_hash);
    if (new_hash == NULL) {
        return (-1);
    }
    new_object_path = checkout_entry_build_object_path(blob_hash);
    if (new_object_path == NULL) {
        free(new_hash);
        return (-1);
    }
    free(entry->blob_hash);
    free(entry->object_path);
    entry->blob_hash = new_hash;
    entry->object_path = new_object_path;
    return (0);
}

int checkout_entry_compare(const checkout_entry *left,
    const checkout_entry *right) {
    if (left == NULL || right == NULL) {
        return (-1);
    }
    if (left->relative_path == NULL || right->relative_path == NULL) {
        return (-1);
    }
    if (left->blob_hash == NULL || right->blob_hash == NULL) {
        return (-1);
    }
    if (strcmp(left->relative_path, right->relative_path) != 0) {
        return (-1);
    }
    if (strcmp(left->blob_hash, right->blob_hash) != 0) {
        return (-1);
    }
    return (0);
}

void checkout_entry_destroy(checkout_entry *entry) {
    if (entry == NULL) {
        return;
    }
    free(entry->relative_path);
    free(entry->blob_hash);
    free(entry->object_path);
    free(entry);
}

static char *checkout_entry_strdup(const char *value) {
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

static char *checkout_entry_build_object_path(const char *blob_hash) {
    char *object_path;
    int needed;

    if (blob_hash == NULL) {
        return (NULL);
    }
    needed = snprintf(NULL, 0, ".mygit/objects/%s", blob_hash);
    if (needed < 0) {
        return (NULL);
    }
    object_path = malloc((size_t)needed + 1);
    if (object_path == NULL) {
        return (NULL);
    }
    snprintf(object_path, (size_t)needed + 1, ".mygit/objects/%s", blob_hash);
    return (object_path);
}
