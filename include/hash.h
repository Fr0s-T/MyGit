#ifndef HASH_H
# define HASH_H

# define SHA1_HEX_LENGTH 40
# define SHA1_HEX_BUFFER_SIZE (SHA1_HEX_LENGTH + 1)

/*
** Computes the SHA-1 hash of a NUL-terminated in-memory string.
**
** Returns:
** - heap-allocated lowercase hex digest on success
** - NULL on invalid input or OpenSSL/allocation failure
**
** Ownership:
** - caller owns the returned string and must free it
*/
char *sha1(const char *input);

/*
** Computes the SHA-1 hash of a file's raw byte contents.
**
** Parameters:
** - out: caller-provided buffer of size SHA1_HEX_BUFFER_SIZE
**
** Returns:
** - 0 on success
** - -1 on invalid input, file-open failure, or digest failure
*/
int hash_file_sha1(const char *file_path, char out[SHA1_HEX_BUFFER_SIZE]);

#endif
