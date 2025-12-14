
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

    
    // if user does not exist, create it
    if(user == NULL){
        pid_t pid = fork();
        printf("Creating user %s\n", username);
        if (pid ==0){
            sys_create_user(username);
            exit(1);
        }
        waitpid(pid, NULL, 0);
    }

    user = getpwnam(username); // recheck if now user exists after creation attempt
    if(user == NULL){
        perror("User creation failed");
        return -1; // user does not exist and creation failed
    } 

    // switch to the user's UID and GID

    uid_t user_uid = user->pw_uid;
    gid_t user_gid = user->pw_gid;

    if (setgid(user_gid) != 0) {
        perror("Errore setgid");
        return -1;
    }

    if (setuid(user_uid) != 0) {
        perror("Errore setuid");
        return -1;
    }


    strncpy(s->username, username, sizeof(s->username)); // set username of the session

    //strncat(s->home_dir, s->root_dir + "/" + s->username, sizeof(s->home_dir)); // set home directory in the format root_dir/username

/*
    DA IMPLEMENTARE LA CREAZIONE DELLA HOME SE NON ESISTE



    sys_make_directory(s->home_dir, 0750);
*/
    strncpy(s->current_dir, s->home_dir, sizeof(s->current_dir)); // set current directory to home directory


    s->logged_in = 1; // mark as logged in

    return 0; // login successful

    } 