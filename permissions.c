
#include "permissions.h"
#include <unistd.h>
#include <stdio.h>
#include <pwd.h>

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
int drop_to_real_user(uid_t real_uid,gid_t real_gid) {

    // if (setgid(real_gid) != 0) {
    //     perror("Error setgid");
    //     return 0;
    // }

    // if (setuid(real_uid) != 0) {
    //     perror("Error setuid");
    //     return 0;
    // }
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

void drop_privileges_to_user(const char *username) {
    struct passwd *user = getpwnam(username);
    if (user == NULL) {
        perror("getpwnam failed");
        return;
    }

    uid_t user_uid = user->pw_uid;
    gid_t user_gid = user->pw_gid;

    if (!drop_to_real_user(user_uid, user_gid)) {
        perror("Failed to drop privileges");
    }
}