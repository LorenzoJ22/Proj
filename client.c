#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


#include "values.h"
#include "network.h"
#include "send_clients_data.h"
#include <sys/select.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>



int can_exit() {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("\nProcess %d terminated\n", pid);
    }
    if (waitpid(-1, NULL, WNOHANG) == 0) {
        return 0; 
    }
    return 1; 
}



int main(int argc, char *argv[]){

    signal(SIGCHLD, SIG_IGN);

    char current_username[64] = {0}; 
    fd_set readfds;
    char buffer[BUFFER_SIZE];  

    //if not enough arguments, use default values
    char *ip = (argc >= 2) ? argv[1] : "127.0.0.1";//argc = 3, argv[0] = main, argv[1]= 127.0.0.1 argv[2]=port
    int port = (argc >= 3) ? atoi(argv[2]) : 8080;


    int sockfd = connect_to_server(ip, port);
    printf("Connesso a %s:%d\n", ip, port);


    while(1){
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (ready < 0) {
            perror("select");
            break;
        }
        
        

        if (FD_ISSET(sockfd, &readfds)) {
            memset(buffer, 0, sizeof(buffer));

            // recv non bloccante (perché sappiamo già che c'è roba)
            int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            
            if (n <= 0) {
                printf("\nConnection closed by server\n");
                break;
            }
            
            buffer[n] = '\0'; // Tappo fondamentale

            // Stampa diretta di qualsiasi cosa arrivi (Risposte, Prompt, Errori)
            printf("%s", buffer);
            fflush(stdout); // Forza la stampa immediata (per il prompt)
        }



        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, sizeof(buffer));

            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break; // CTRL+D
            }

            buffer[strcspn(buffer, "\n")] = 0;  // rimuove newline

            // handler special commands
            if (strlen(buffer) == 0){
                send_message(sockfd, ""); 
                continue;
            } 

            if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "exit ") == 0) {
                if (can_exit()) {
                    printf("Exit in progress. Goodbye!\n");
                    close(sockfd);
                    exit(0);
                } else {
                    // C'è roba in background -> Blocchiamo l'uscita
                    printf(COLOR_RED "Error: There are background operations (upload/download) still active.\n" COLOR_RESET);
                    printf("Please wait for their completion before exiting.\n");
                    continue;
                }
            }

            if (strncmp(buffer, "login", 5) == 0) {
                sscanf(buffer + 6, "%s", current_username); 
            }


            if (strncmp(buffer, "upload", 6) == 0) {
                client_upload(sockfd, buffer, ip, port, current_username);
                continue;
            }

            if (strncmp(buffer, "download", 8) == 0) {

                client_download(sockfd, buffer, ip, port, current_username);
                sleep(0.5);
                continue;
            }

            if (strncmp(buffer, "write ", 6) == 0) {
            // If so, I enter "Send File" mode.
            // I do NOT have to immediately go back to the beginning of the while loop to do receive_message!
            send_message(sockfd, buffer);
            client_write_data(sockfd,buffer); 
            continue;
            }

            if (strncmp(buffer, "read ", 5) == 0) {
            send_message(sockfd, buffer);
            client_read_data(sockfd,buffer); 
            continue;
            }
            
            send_message(sockfd, buffer);
        }

    }
    

    

    close(sockfd);
    return 0;
}