#include "client_handler.h"
#include "session.h"
#include "system_ops.h"
#include "values.h"
#include "permissions.h"
#include "commands_client.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>



void send_prompt(int client_fd, Session *s) {
    char prompt[4190];
    
    if (s->logged_in) {
        // Formato: username@shell:/path/corrente$ 
        char cwd[PATH_MAX];
        snprintf(prompt, sizeof(prompt), "\033[1;32m%s@shell\033[0m:\033[1;34m%s\033[0m$ ", 
                 s->username, /* s->current_dir */ getcwd(cwd, PATH_MAX));
    } else {
        // Formato: guest:/path/corrente>
        snprintf(prompt, sizeof(prompt), "guest:%s> ", s->current_dir);
    }
    
    write(client_fd, prompt, strlen(prompt));
}


void handle_client(int client_fd, const char *root_dir) {

    Session s;
    session_init(&s, root_dir);

    char buffer[1024];

    
    //send_prompt(client_fd, &s);

    
    while (1) {

        memset(buffer, 0, sizeof(buffer)); // clear buffer
        send_prompt(client_fd, &s);
        int n = read(client_fd, buffer, sizeof(buffer)-1);
        if (n <= 0) break; // connection closed or error
        //buffer[n] = '\0'; //we have to terminate with \0 because read didn't put it
        printf("[FIGLIO %d] Messaggio ricevuto: %s\n", getpid(), buffer);
        printf("[DEBUG] Buffer pulito: '%s' (lunghezza: %lu)\n", buffer, strlen(buffer));
        //buffer[strcspn(buffer, "\n\r")] = 0;

         if (strlen(buffer) == 0) {
            dprintf(client_fd,"Void space: '%s'\n", buffer);
            char b[PATH_MAX];
            dprintf(client_fd, "Adesso sei in: %s\n",getcwd(b,PATH_MAX));
            continue; 
        } 

        // login command
        if (strncmp(buffer, "login ", 6) == 0) {
            login(buffer, client_fd, &s);
            continue;
        }
        

        // create_user command
        if (strncmp(buffer, "create_user ", 12) ==0){
            create_user(client_fd, buffer, &s);
            continue;
        }

        /*Add here the if to check if the current user is logged in, so we don't have to repetly put it in every function*/

        //create [-d] <path><permission> command, create a file 
        if(strncmp(buffer, "create ", 7) ==0){
            create(client_fd, buffer, &s);
            continue;
        }

        if((strncmp(buffer, "cd ", 3)==0) || strncmp(buffer, "cd", 2)==0){
            change_directory(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "chmod ", 6)==0){
            chmods(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "move ", 5)==0){
            move(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "list ", 5)==0 || strncmp(buffer, "list", 4)==0){
            list(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "delete ", 7)==0){
            delete(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "write ", 6)==0){
            write_client(client_fd, buffer, &s);
            continue;
        }


        //if not logged in, only allow login command
        if (!s.logged_in) {
            char msg[] = "Login first or create user command\n";
            write(client_fd, msg, strlen(msg));
            continue;
        }

        
        // unknown command
        char msg[] = "Unknown command \n";
        write(client_fd, msg, strlen(msg));
        
        //dprintf(client_fd,"The input was buffer = '%s'\n", buffer);
        /* char terminator = '\0'; 
        write(client_fd, &terminator, 1); */
        //memset(buffer, 0, sizeof(buffer));
        //send_prompt(client_fd, &s);

        
    }

    

    // read message
    int n = read(client_fd, buffer, sizeof(buffer));
    if (n > 0) {
        printf("[FIGLIO %d] Messaggio ricevuto da fine: %s\n", getpid(), buffer);
    }

    // send response
    char response[] = "Ciao dal server!";
    write(client_fd, response, strlen(response) + 1);

    close(client_fd);
}