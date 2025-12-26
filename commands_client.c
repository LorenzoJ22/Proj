#include "client_handler.h"
#include "session.h"
#include "system_ops.h"
#include "values.h"
#include "permissions.h"

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>



void login( char *buffer, int client_fd, Session *s){
    
            
             // send login result to client
                char username[64];
                sscanf(buffer + 6, "%63s", username);
                int login_result = session_login(s, username);
            if (login_result == 0) {
                char msg[] = "Login successful\n";
                write(client_fd, msg, strlen(msg));
                memset(buffer, 0, sizeof(&buffer));
                //send_prompt(client_fd, s);
            } 
            else
             if (login_result == -2) {
                char msg[] = "Already logged in\n";
                write(client_fd, msg, strlen(msg));
            } 
            else {
                char msg[] = "Login failed\n";
                write(client_fd, msg, strlen(msg));
            }

}


void create_user(int client_fd, char *buffer, Session *s){
    // check if logged in 
            if (s->logged_in) {
            char msg[] = "Cannot create user while logged in\n";
            write(client_fd, msg, strlen(msg));
            return;
            }

            char username[64];  
            char perm_str[8];

           if (sscanf(buffer + 12, "%63s %7s", username, perm_str) != 2) {
            char msg[] = "Use: create_user <username> <permissions>\n";
                write(client_fd, msg, strlen(msg));
                return;
            }

            mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t

            if(ensure_user_exists(username)){
                char msg[] = "User already exists\n";
                write(client_fd, msg, strlen(msg));
                return;
            }

            if (sys_create_user(username) == -1){
                char msg[] = "User creation failed\n";
                write(client_fd, msg, strlen(msg));
                return;
            }

            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", s->root_dir, username);


            sys_make_directory(full_path, perms, GROUP_NAME,username);  

            char msg[] = "User created successfully\n";
            write(client_fd, msg, strlen(msg));
            return;
}

void create(int client_fd, char *buffer, Session *s){
            
            if (!(s->logged_in)) {
                char msg[] = "Cannot create file while you are guest\n";
                write(client_fd, msg, strlen(msg));
                return;
            }
            
            
            int is_dir = 0; // 0 = file, 1 = directory
            // Pointer that begin after the "create " (quindi a buffer + 7)
            char *args = buffer + 7;
            
            // Search if the next characters are "-d " (note the space after d)
            if (strncmp(args, "-d ", 3) == 0) {
                is_dir = 1;      // We found the option!
                args += 3;       // Shift the pointer of three positions (hop "-d ")
                // Now args point to the beginnig of the path
                }
               
               char path[64];  
               char perm_str[8];

            if (sscanf(args, "%63s %7s", path, perm_str) != 2 || strncmp(path, ".",1) == 0 || strncmp(path, "..",2) == 0) {
                char msg[] = "Use: create [-d] <path> <permissions>\n";
                write(client_fd, msg, strlen(msg));
                return;
            }

            mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t

            int is_absolute = 0;
            if (strncmp(path, "root", 4) == 0) {
                if (path[4] == '\0' || path[4] == '/') {
                is_absolute = 1;
               }
            }
               
               size_t needed_len = strlen(s->current_dir) + strlen(path) + 2; 
               
               char full_path[6000];//il full_path inizializzato con 4096 ha la stessa misura del current_dir quindi nel peggiore dei casi con snprintf la tronca uscendo dalla stringa
               /*^ we have increase this parameter to not have warnings^*/
               if (is_absolute) {
                   //CASE ABSOLUTE: path already begin with "root/..."
                   if (strlen(path) >= PATH_MAX) {
                       char msg[] = "Error: Path too long\n";
                       write(client_fd, msg, strlen(msg));
                       return;
                   }
                   strncpy(full_path, path, PATH_MAX - 1);
               } else {
                //CASE RELATIVE: left the path without change

            if (needed_len > PATH_MAX) {//check if he is arrive to the max 
                char msg[] = "Error: Path too long\n";
                write(client_fd, msg, strlen(msg));
                return;
            }
            snprintf(full_path, sizeof(full_path), "%s/%s", s->current_dir, path);
  
        }
            if(is_dir){
                sys_make_directory(full_path, perms, GROUP_NAME,s->username);  
                char msg[] = "Directory created successfully\n";
                write(client_fd, msg, strlen(msg));
                return;
            }
            else{                
                printf("Full path: %s\n", full_path);
                
            int result = sys_make_file(full_path, perms, GROUP_NAME, s->username);  
                printf("Errno number: %d\n", EEXIST);
                if(result == -EEXIST){
                    char msg[] = "File already created\n";
                    write(client_fd, msg, strlen(msg));
                    return;
                }else{
                    char msg[] = "File created successfully\n";
                    write(client_fd, msg, strlen(msg));
                    return;
                }
            }

}




