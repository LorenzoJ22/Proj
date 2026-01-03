#ifndef SYSTEM_OPS_H
#define SYSTEM_OPS_H

#include <sys/types.h>
#include "session.h"

int sys_create_user(const char *username);

void sys_make_directory(const char *path, mode_t mode, const char *groupname, const char *username);
int sys_make_file(const char *path, mode_t mode, const char *groupname, const char *username);
int sys_change_owner(const char *path, uid_t uid, gid_t gid);
int ensure_user_exists(const char *username);
int create_group(const char *groupname);
//void change_dir(const char *path, int client_fd, Session *s);
void check_full_path(int client_fd, char *path, Session *s, char *full_path);
void normalize_path(char *path, Session *s);
#endif