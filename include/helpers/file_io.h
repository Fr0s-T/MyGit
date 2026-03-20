#ifndef MYGIT_HELPERS_FILE_IO_H
#define MYGIT_HELPERS_FILE_IO_H

#include "../hash.h"

/*
** Small reusable file I/O helpers.
**
** These helpers are intentionally not wired into the current code yet.
** They exist as a proposed abstraction layer so the rest of the project
** can stop repeating fopen/fclose/read/write boilerplate in the future.
**
** Intended use cases:
** - read HEAD or a branch ref in one call
** - write a small text file in one call
** - copy a file into the objects directory in one call
** - query the index for a single tracked hash in one call
*/

/*
** Reads the first line from a text file.
**
** Returns:
**  0  on success
** -1  on failure
**
** Behavior:
** - trims trailing '\n' and '\r'
** - allocates *out on success
** - caller must free(*out)
*/
int file_io_read_first_line(const char *path, char **out);

/*
** Writes a full text buffer into a file, replacing existing content.
**
** Returns:
**  0  on success
** -1  on failure
*/
int file_io_write_text(const char *path, const char *content);

/*
** Copies a file byte-for-byte from src_path to dst_path.
**
** Returns:
**  0  on success
** -1  on failure
**
** If writing fails after the destination file is created,
** the partially written destination file is removed.
*/
int file_io_copy_file(const char *src_path, const char *dst_path);

/*
** Reads a single index entry by file path from a tab-separated index file.
**
** The expected index format is:
** path<TAB>hash<NEWLINE>
**
** Returns:
**  1  if the path exists in the index and out_hash was filled
**  0  if the path does not exist in the index
** -1  on parsing or file errors
*/
int file_io_read_index_hash(const char *index_path, const char *file_path,
    char out_hash[SHA1_HEX_BUFFER_SIZE]);

#endif
