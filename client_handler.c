#include "client_handler.h"
#include "session.h"
#include "system_ops.h"
#include "values.h"
#include "permissions.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>




void handle_client(int client_fd, const char *root_dir) {

    Session s;
    session_init(&s, root_dir);

    char buffer[1024];


    while (1) {

        memset(buffer, 0, sizeof(buffer)); // clear buffer
        int n = read(client_fd, buffer, sizeof(buffer));
        if (n <= 0) break; // connection closed or error

        printf("[FIGLIO %d] Messaggio ricevuto: %s\n", getpid(), buffer);

        

        // login command
        if (strncmp(buffer, "login ", 6) == 0) {
            char username[64];
            sscanf(buffer + 6, "%63s", username);
            int login_result = session_login(&s, username);

            // send login result to client

            if (login_result == 0) {
                char msg[] = "Login successful\n";
                write(client_fd, msg, strlen(msg));
            } 
            else if (login_result == -2) {
                char msg[] = "Already logged in\n";
                write(client_fd, msg, strlen(msg));
            } 
            else {
                char msg[] = "Login failed\n";
                write(client_fd, msg, strlen(msg));
            }


            

            continue;


            


        }


        // create_user command
        if (strncmp(buffer, "create_user ", 12) ==0){

            // check if logged in 
            if (s.logged_in) {
            char msg[] = "Cannot create user while logged in\n";
            write(client_fd, msg, strlen(msg));
            continue;
            }


            char username[64];  
            char perm_str[8];

           if (sscanf(buffer + 12, "%63s %7s", username, perm_str) != 2) {
            char msg[] = "Use: create_user <username> <permissions>\n";
                write(client_fd, msg, strlen(msg));
                continue;
            }

            mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t

            if(ensure_user_exists(username)){
                char msg[] = "User already exists\n";
                write(client_fd, msg, strlen(msg));
                continue;
            }

            if (sys_create_user(username) == -1){
                char msg[] = "User creation failed\n";
                write(client_fd, msg, strlen(msg));
                continue;
            }

            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", root_dir, username);



            pid_t pid = fork();
            if (pid < 0) {
                perror("Fork failed");
                exit(1);
            }

            if (pid == 0) { // child process
                drop_privileges_to_user(username);
                sys_make_directory(full_path, perms, GROUP_NAME);

                
                exit(0);
            }

            waitpid(pid, NULL, 0); // parent waits for child to finish

           

            

            

            

            




            char msg[] = "User created successfully\n";
            write(client_fd, msg, strlen(msg));
            continue;
        }



        //if not logged in, only allow login command
        if (!s.logged_in) {
            char msg[] = "Login first \n";
            write(client_fd, msg, strlen(msg));
            continue;
        }

        
        // unknown command
        char msg[] = "Unknown command \n";
        write(client_fd, msg, strlen(msg));


    }










    

    // read message
    int n = read(client_fd, buffer, sizeof(buffer));
    if (n > 0) {
        printf("[FIGLIO %d] Messaggio ricevuto: %s\n", getpid(), buffer);
    }

    // send response
    char response[] = "Ciao dal server!";
    write(client_fd, response, strlen(response) + 1);

    close(client_fd);
}