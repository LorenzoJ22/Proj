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
        snprintf(prompt, sizeof(prompt), "\033[1;32m%s@shell\033[0m:\033[1;34m%s\033[0m$ ", 
                 s->username, s->current_dir);
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

    
    send_prompt(client_fd, &s);

    while (1) {

        memset(buffer, 0, sizeof(buffer)); // clear buffer
        int n = read(client_fd, buffer, sizeof(buffer));
        if (n <= 0) break; // connection closed or error

        printf("[FIGLIO %d] Messaggio ricevuto: %s\n", getpid(), buffer);
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



        //if not logged in, only allow login command
        if (!s.logged_in) {
            char msg[] = "Login first or create user command\n";
            write(client_fd, msg, strlen(msg));
            continue;
        }

        
        // unknown command
        char msg[] = "Unknown command \n";
        write(client_fd, msg, strlen(msg));

        memset(buffer, 0, sizeof(buffer));
        send_prompt(client_fd, &s);

        
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