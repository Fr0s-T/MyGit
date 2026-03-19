#include "../include/my_includes.h"

int		create_blob(file_data *file, char *git_obj_path);
char	*create_git_dir(char *cwd);
int		blob_exists(char *file_hash, char *git_obj_path);
int		copy_file_content(char *src_path, char *dst_path);

int create_blob_and_indexing(file_data **files, int len_file, char *cwd) {
	char	*git_obj_path;

	git_obj_path = create_git_dir(cwd);
	if (git_obj_path == NULL) {
		printf("[create_blob_and_indexing]" C_RED " Failed to create git objects path\n" C_RESET);
		return (-1);
	}
	for (int i = 0; i < len_file; i++) {
		file_data *file = files[i];

		if (create_blob(file, git_obj_path) != 0) {
			printf("[Blob creation]" C_RED " Failed to create blob for: %s\n" C_RESET, file->path);
		}
	}
	free(git_obj_path);
	return (0);
}

char *create_git_dir(char *cwd) {
	return (generate_path(cwd, "/.mygit/objects"));
}

int create_blob(file_data *file, char *git_obj_path) {
	char	blob_path[PATH_MAX];

	if (blob_exists(file->hash, git_obj_path) == 0) {
		return (0);
	}
	if (snprintf(blob_path, sizeof(blob_path), "%s/%s", git_obj_path, file->hash) >= (int)sizeof(blob_path)) {
		printf("[create_blob]" C_RED " Blob path too long for: %s\n" C_RESET, file->path);
		return (-1);
	}
	if (copy_file_content(file->path, blob_path) != 0) {
		printf("[create_blob]" C_RED " Failed copying data for: %s\n" C_RESET, file->path);
		return (-1);
	}
	return (0);
}

int copy_file_content(char *src_path, char *dst_path) {
	FILE	*src_file;
	FILE	*dst_file;
	char	buffer[4096];
	size_t	bytes_read;
	size_t	bytes_written;

	src_file = fopen(src_path, "rb");
	if (src_file == NULL) {
		printf("[copy_file_content]" C_RED " Failed to open source file: %s\n" C_RESET, src_path);
		return (-1);
	}
	dst_file = fopen(dst_path, "wb");
	if (dst_file == NULL) {
		printf("[copy_file_content]" C_RED " Failed to open destination file: %s\n" C_RESET, dst_path);
		fclose(src_file);
		return (-1);
	}
	bytes_read = fread(buffer, 1, sizeof(buffer), src_file);
	while (bytes_read > 0) {
		bytes_written = fwrite(buffer, 1, bytes_read, dst_file);
		if (bytes_written != bytes_read) {
			printf("[copy_file_content]" C_RED " Failed writing to blob: %s\n" C_RESET, dst_path);
			fclose(src_file);
			fclose(dst_file);
			remove(dst_path);
			return (-1);
		}
		bytes_read = fread(buffer, 1, sizeof(buffer), src_file);
	}
	if (ferror(src_file)) {
		printf("[copy_file_content]" C_RED " Failed reading source file: %s\n" C_RESET, src_path);
		fclose(src_file);
		fclose(dst_file);
		remove(dst_path);
		return (-1);
	}
	fclose(src_file);
	fclose(dst_file);
	return (0);
}

int blob_exists(char *file_hash, char *git_obj_path) {
	DIR				*current_dir;
	struct dirent	*entry;

	current_dir = opendir(git_obj_path);
	if (current_dir == NULL) {
		printf("\n[blob_exists]" C_RED " Failed to open dir: %s\n" C_RESET, git_obj_path);
		return (-1);
	}
	while ((entry = readdir(current_dir)) != NULL) {
		if (strcmp(file_hash, entry->d_name) == 0) {
			closedir(current_dir);
			return (0);
		}
	}
	closedir(current_dir);
	return (1);
}