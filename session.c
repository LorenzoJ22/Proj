
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <pwd.h>
#include <errno.h>
#include <grp.h>

#include "system_ops.h"
#include "session.h"
#include "permissions.h"



int check_user_login(const char *username, const char *groupname ){

    struct group *grp = getgrnam(groupname);
    gid_t target_gid = grp->gr_gid;

    struct passwd *pw = getpwnam(username);
    if (pw == NULL) {
        return 0; // Utente non esiste
    }

    int ngroups = 0;
    getgrouplist(username, pw->pw_gid, NULL, &ngroups);

    gid_t *groups = malloc(ngroups * sizeof(gid_t));
    if (groups == NULL) return 0;

    if (getgrouplist(username, pw->pw_gid, groups, &ngroups) == -1) {
        free(groups);
        return 0;
    }
    

    int found = 0;
    for (int i = 0; i < ngroups; i++) {
        if (groups[i] == target_gid) {
            found = 1;
            break;
        }
    }

    free(groups);
    return found;
}


void session_init(Session *s, const char *root) {

    s->logged_in = 0; //at start, user is not logged in
    strncpy(s->root_dir, root, sizeof(s->root_dir)-1); //set root directory
    s->username[0] = '\0'; 
    s->current_dir[0] = '\0';
    s->home_dir[0] = '\0';
    
}

int session_login(Session *s, const char *username) {

    if (s->logged_in) {
        
        return -2; // already logged in
    }

    printf("Attempting login for user: %s\n", username);

    struct passwd *user = getpwnam(username);

    if (!check_user_login(username, GROUP_NAME)) {
        printf("Login failed: User '%s' does not exist or is not in the group '%s'\n", username, GROUP_NAME);
        return -1; // login failed
    }
    //aggiunta controllo home se esiste

    // switch to the user's UID and GID

    uid_t user_uid = user->pw_uid;
    gid_t user_gid = user->pw_gid;
    
   // trap the process in the root/jail
    char full_path[PATH_MAX+3000];
    char b[PATH_MAX];
    if (getcwd(b, PATH_MAX) != NULL) {
    snprintf(full_path, PATH_MAX+3000, "%s/%s", b, s->root_dir);
    }else {
    perror("getcwd error");
    }
    printf("path corrente %s\n", b);
    printf("full_path %s\n", full_path);
    
    if(chdir(full_path)== -1){
        printf("error: %s\n", strerror(errno));
    }
    if(chroot(full_path)== -1){
    printf("error chroot: %s\n", strerror(errno));
    }
    


    drop_to_real_user(user_uid, user_gid); // ensure we are not root before switching

    strncpy(s->username, username, sizeof(s->username)); // set username of the session
    printf("Set session username to: %s\n", s->username);

    snprintf(s->home_dir, sizeof(s->home_dir), "%s/%s", s->root_dir, username); // set home directory
    printf("Set home directory to: %s\n", s->home_dir);


    strncpy(s->current_dir, s->home_dir, sizeof(s->current_dir)); // set current directory to home directory
    printf("Set current directory to: %s\n", s->current_dir);


    s->logged_in = 1; // mark as logged in

    return 0; // login successful

    } 

    /*int session_update_dir(Session *s, const char *path){
        strncpy(s->parent_dir, s->current_dir, sizeof(s->current_dir));
        strncpy(s->current_dir, path, sizeof(s->current_dir)); // set current directory to home directory
        printf("Set current directory to: %s\n", s->current_dir);

         

}*/