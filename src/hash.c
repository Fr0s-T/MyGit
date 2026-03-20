#include "../include/my_includes.h"
#include <openssl/evp.h>

enum e_hash_constants {
    HASH_READ_BUFFER_SIZE = 4096,
};

static void bytes_to_hex(const unsigned char *bytes, unsigned int len,
    char out[SHA1_HEX_BUFFER_SIZE]);

char *sha1(const char *input) {
    EVP_MD_CTX *ctx;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    char *out;

    if (!input)
        return (NULL);
    ctx = EVP_MD_CTX_new();
    if (!ctx)
        return (NULL);
    out = malloc(SHA1_HEX_BUFFER_SIZE);
    if (!out) {
        EVP_MD_CTX_free(ctx);
        return (NULL);
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        free(out);
        return (NULL);
    }
    if (EVP_DigestUpdate(ctx, input, strlen(input)) != 1) {
        EVP_MD_CTX_free(ctx);
        free(out);
        return (NULL);
    }
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        free(out);
        return (NULL);
    }
    bytes_to_hex(digest, digest_len, out);
    EVP_MD_CTX_free(ctx);
    return (out);
}

int hash_file_sha1(const char *file_path, char out[SHA1_HEX_BUFFER_SIZE]) {
    FILE *fp;
    EVP_MD_CTX *ctx;
    unsigned char buffer[HASH_READ_BUFFER_SIZE];
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    size_t bytes_read;
    int status;

    if (!file_path || !out)
        return (-1);
    fp = fopen(file_path, "rb");
    if (!fp)
        return (-1);
    ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fclose(fp);
        return (-1);
    }
    status = -1;
    if (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) != 1)
        goto cleanup;
    bytes_read = fread(buffer, 1, sizeof(buffer), fp);
    while (bytes_read > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytes_read) != 1)
            goto cleanup;
        bytes_read = fread(buffer, 1, sizeof(buffer), fp);
    }
    if (ferror(fp))
        goto cleanup;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1)
        goto cleanup;
    bytes_to_hex(digest, digest_len, out);
    status = 0;
cleanup:
    EVP_MD_CTX_free(ctx);
    fclose(fp);
    return (status);
}

static void bytes_to_hex(const unsigned char *bytes, unsigned int len,
    char out[SHA1_HEX_BUFFER_SIZE]) {
    const char *hex;
    unsigned int i;

    hex = "0123456789abcdef";
    i = 0;
    while (i < len) {
        out[i * 2] = hex[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[bytes[i] & 0xF];
        i++;
    }
    out[len * 2] = '\0';
}
