#ifndef MYGIT_SERVICES_H
#define MYGIT_SERVICES_H

int create_directory(const char *path);
char *generate_path(const char *base, const char *extension);
int create_empty_file(const char *file_path_with_name_included);

#endif