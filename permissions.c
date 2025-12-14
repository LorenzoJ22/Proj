
#include "permissions.h"
#include <unistd.h>
#include <stdio.h>

// static variables to hold the IDs
static uid_t real_uid;
static gid_t real_gid;
static uid_t saved_euid;
static gid_t saved_egid;

// save current real and effective user and group IDs
void save_ids() {
    real_uid  = getuid();
    real_gid  = getgid();
    saved_euid = geteuid();
    saved_egid = getegid();
}

// elevate to root privileges
int elevate_to_root() {
    if (geteuid() == 0) return 1; // already root

    if (setegid(0) != 0) {
        perror("setegid(0)");
        return 0;
    }
    if (seteuid(0) != 0) {
        perror("seteuid(0)");
        return 0;
    }
    return 1;
}

// drop to real user privileges
int drop_to_real_user() {
    if (setegid(real_gid) != 0) {
        perror("setegid(real_gid)");
        return 0;
    }
    if (seteuid(real_uid) != 0) {
        perror("seteuid(real_uid)");
        return 0;
    }
    return 1;
}
