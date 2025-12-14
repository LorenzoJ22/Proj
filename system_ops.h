#ifndef SYSTEM_OPS_H
#define SYSTEM_OPS_H

#include <sys/types.h>


void sys_create_user(const char *username);

void sys_make_directory(const char *path, mode_t mode);
int sys_change_owner(const char *path, uid_t uid, gid_t gid);
int ensure_group_exists(const char *groupname);

#endif