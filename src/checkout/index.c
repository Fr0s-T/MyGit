#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "checkout_index.h"

static void destroy_checkout_entries(checkout_entry **entries, int count);

int checkout_collect_current_tracked_entries(checkout_entry ***current_entries,
    int *entry_count) {
    FILE *index_file;
    char line[4096 + SHA1_HEX_BUFFER_SIZE + 8];

    if (current_entries == NULL || entry_count == NULL) {
        return (-1);
    }
    *current_entries = NULL;
    *entry_count = 0;
    index_file = fopen(".mygit/index", "r");
    if (index_file == NULL) {
        return (-1);
    }
    while (fgets(line, sizeof(line), index_file) != NULL) {
        char *tab;
        char *line_end;
        checkout_entry **new_entries;
        checkout_entry *new_entry;
        size_t path_len;
        size_t hash_len;
        char *path;
        char *hash;

        tab = strchr(line, '\t');
        if (tab == NULL) {
            continue;
        }
        line_end = strpbrk(tab + 1, "\r\n");
        if (line_end == NULL) {
            line_end = line + strlen(line);
        }
        path_len = (size_t)(tab - line);
        hash_len = (size_t)(line_end - (tab + 1));
        if (path_len == 0 || hash_len == 0) {
            continue;
        }
        path = malloc(path_len + 1);
        hash = malloc(hash_len + 1);
        if (path == NULL || hash == NULL) {
            free(path);
            free(hash);
            fclose(index_file);
            destroy_checkout_entries(*current_entries, *entry_count);
            *current_entries = NULL;
            *entry_count = 0;
            return (-1);
        }
        memcpy(path, line, path_len);
        path[path_len] = '\0';
        memcpy(hash, tab + 1, hash_len);
        hash[hash_len] = '\0';
        new_entry = checkout_entry_create(path, hash);
        free(path);
        free(hash);
        if (new_entry == NULL) {
            fclose(index_file);
            destroy_checkout_entries(*current_entries, *entry_count);
            *current_entries = NULL;
            *entry_count = 0;
            return (-1);
        }
        new_entries = realloc(*current_entries,
            (size_t)(*entry_count + 1) * sizeof(checkout_entry *));
        if (new_entries == NULL) {
            checkout_entry_destroy(new_entry);
            fclose(index_file);
            destroy_checkout_entries(*current_entries, *entry_count);
            *current_entries = NULL;
            *entry_count = 0;
            return (-1);
        }
        *current_entries = new_entries;
        (*current_entries)[*entry_count] = new_entry;
        (*entry_count)++;
    }
    fclose(index_file);
    return (0);
}

static void destroy_checkout_entries(checkout_entry **entries, int count) {
    if (entries == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        checkout_entry_destroy(entries[i]);
    }
    free(entries);
}
