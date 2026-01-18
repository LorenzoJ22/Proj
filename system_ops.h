#ifndef SYSTEM_OPS_H
#define SYSTEM_OPS_H

#include <sys/types.h>
#include "session.h"

int sys_create_user(const char *username);

void sys_make_directory(const char *path, mode_t mode, const char *groupname, const char *username);
int sys_make_file(const char *path, mode_t mode);
int sys_change_owner(const char *path, uid_t uid, gid_t gid);
int ensure_user_exists(const char *username);
int create_group(const char *groupname);

int check_home_violation(char *resolved_path, int client_fd, Session *s);
void move_file(int client_fd, const char *source_path, const char *dest_dir);
void get_perm_string(mode_t mode, char *str);
long long get_directory_content_size(const char *path);
void sys_make_directory_creat(const char *path, mode_t mode);
char *get_last(char *path, int client_fd);
#endif