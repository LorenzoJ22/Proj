#ifndef SESSION_H
#define SESSION_H

#include <limits.h>

// Structure to hold session information of a connected client

typedef struct {
    int logged_in; // 0 = not logged in, 1 = logged in
    char username[64]; 
    char current_dir[PATH_MAX]; // current working directory
    char home_dir[PATH_MAX]; // home directory
    char root_dir[PATH_MAX]; // root directory
} Session;



void session_init(Session *s, const char *root); // Initialize session


int session_login(Session *s, const char *username); // Log in user

#endif