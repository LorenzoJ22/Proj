#include "system_ops.h"
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void sys_create_user(const char *username) {
   
    execlp("adduser", "adduser", "--disabled-password","--gecos" ,"", username, NULL);
    perror("exec adduser");
    _exit(1);

}

void sys_make_directory(const char *path, mode_t mode) {

    struct stat st;
    if (stat(path, &st) == -1) {
        printf("Creating directory\n");
        if (mkdir(path, mode) < 0) {
            perror("Failed to create directory");
            exit(1);
        }
    }

    
}

int ensure_group_exists(const char *groupname) {
    //struct group *g = getgrnam(groupname);
    //if (g != NULL) return 1; // group exists

    pid_t pid = fork();
    if (pid == 0) {
        execlp("groupadd", "groupadd", groupname, (char*)NULL);
        perror("exec groupadd");
        exit(1);
    }
    return 1;
}