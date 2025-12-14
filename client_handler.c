#include "client_handler.h"
#include "session.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>

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

        //if not logged in, only allow login command
        if (!s.logged_in) {
            char msg[] = "Login first \n";
            write(client_fd, msg, strlen(msg));
            continue;
        }


        // comando unknown
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