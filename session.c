
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <pwd.h>

#include "system_ops.h"
#include "session.h"
#include "permissions.h"


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

    struct passwd *user= getpwnam(username); // check if user exists

    printf("User lookup result: %s\n", user ? "found" : "not found");

    if (user == NULL) {
        return -1; // user does not exist
    }

    // switch to the user's UID and GID

    uid_t user_uid = user->pw_uid;
    gid_t user_gid = user->pw_gid;

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