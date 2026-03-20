#ifndef HASH_H
# define HASH_H

# define SHA1_HEX_LENGTH 40
# define SHA1_HEX_BUFFER_SIZE (SHA1_HEX_LENGTH + 1)

int hash_file_sha1(const char *file_path, char out[SHA1_HEX_BUFFER_SIZE]);

#endif
