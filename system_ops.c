#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

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

void sys_make_directory(const char *path, mode_t mode, const char *groupname, const char *username) {

    struct group *grp = getgrnam(groupname);
    struct passwd *pwd = getpwnam(username);
    if (grp == NULL || pwd == NULL) {
        perror("Error: Group or user not found");
        exit(1);
    }

    gid_t new_gid = grp->gr_gid;


    struct stat st;
    if (stat(path, &st) == -1) {
        //umask(002); // clear umask to set exact permissions
        printf("Creating directory at %s\n", path);
        if (mkdir(path, mode) < 0) {
            perror("Failed to create directory");
            exit(1);
        }
    }
        printf("Directory already created\n");

    if (chown(path, pwd->pw_uid, new_gid) == -1) {
        perror("Error changing group ownership");
        exit(1);
    }

    //umask(022); // reset umask

    
}

int sys_make_file(const char *path, mode_t mode, const char *groupname, const char *username) {

    struct group *grp = getgrnam(groupname);
    struct passwd *pwd = getpwnam(username);
    if (grp == NULL || pwd == NULL) {
        perror("Error: Group or user not found");
        exit(1);
    }

    gid_t new_gid = grp->gr_gid;

    struct stat st;
    //if (stat(path, &st) == -1) { //it is not necessary until we handle this case in the errno of the open function
        umask(002); // clear umask to set exact permissions
        printf("Creating file at %s\n", path);
        int fd;
        if ((fd = open(path, O_CREAT | O_RDWR | O_EXCL, mode)) < 0) {
        if (errno == EEXIST) {
            fprintf(stderr, "Error: file '%s' already exists.\n", path);
            return -errno;//alternative we can pass the client file in the sys_make_file in order to write message of warnings to client shell like "file already exists" 
        } 
        else if (errno == EACCES) {
            fprintf(stderr, "Error: Permession denied to create file at '%s'.\n", path);
            exit(1);
        }else{
            perror("Failed to create file"); 
            exit(1);
            }
        } 
        close(fd); 
        printf("File created with permission \n.");
    //}

    //printf("File is already created\n");

    if (chown(path, pwd->pw_uid, new_gid) == -1) {
        perror("Error changing group ownership");
        exit(1);
    }

    umask(022); // reset umask

    return 0;
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


