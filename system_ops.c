#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "system_ops.h"
#include "values.h"

int ensure_user_exists(const char *username) {

    struct passwd *user= getpwnam(username);

   return (user != NULL);
}

int sys_create_user(const char *username) {

    pid_t pid = fork();

    if (pid < 0) {
        // error handling
        perror("Error forking process to create group");
        return -1;
    }

    if (pid == 0) {
        execlp("adduser", "adduser", "--disabled-password","--no-create-home","--gecos" ,"","--ingroup", GROUP_NAME, username, NULL);
    perror("exec adduser");
    _exit(1);
    }

    waitpid(pid, NULL, 0);
    return 1;
}

void sys_make_directory(const char *path, mode_t mode, const char *groupname) {

    struct group *grp = getgrnam(groupname);
    if (grp == NULL) {
        perror("Error: Group not found");
        exit(1);
    }

    gid_t new_gid = grp->gr_gid;


    struct stat st;
    if (stat(path, &st) == -1) {
        umask(002); // clear umask to set exact permissions
        printf("Creating directory at %s\n", path);
        if (mkdir(path, mode) < 0) {
            perror("Failed to create directory");
            exit(1);
        }
    }

    if (chown(path, -1, new_gid) == -1) {
        perror("Error changing group ownership");
        exit(1);
    }

    umask(022); // reset umask

    
}


int create_group(const char *groupname) {

    struct group *g = getgrnam(groupname);
    if (g != NULL) return 1; // group exists

    pid_t pid = fork();

    if (pid < 0) {
        // error handling
        perror("Error forking process to create group");
        return -1;
    }

    if (pid == 0) {
        execlp("groupadd", "groupadd", groupname, (char*)NULL);
        perror("exec groupadd");
        exit(1);
    }

    waitpid(pid, NULL, 0);
    return 1;
}