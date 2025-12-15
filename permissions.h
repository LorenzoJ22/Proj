#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include <sys/types.h>

// save current real and effective user and group IDs
void save_ids();

// Elevate to root privileges
int elevate_to_root();

// drop to real user privileges
int drop_to_real_user(uid_t real_uid, gid_t real_gid);

#endif
