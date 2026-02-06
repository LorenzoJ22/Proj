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

    // 1. PULIZIA (The Reaper): Raccogliamo tutti i processi che sono GIÀ finiti.
    // Se non lo facciamo, un processo finito ma non "raccolto" (zombie) 
    // sembrerebbe ancora esistente e bloccherebbe l'exit.
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("\n[BG] Nota: Il processo background %d è terminato proprio ora.\n", pid);
    }

    // 2. CONTROLLO VIVI
    // Chiamiamo waitpid ancora una volta.
    // Se restituisce 0: Significa "Esistono figli, ma sono ancora vivi e vegeti".
    // Se restituisce -1: Significa "Non ho più figli (ECHILD)".
    
    if (waitpid(-1, NULL, WNOHANG) == 0) {
        // Return 0: C'è ancora qualcuno vivo!
        return 0; // FALSE: Non puoi uscire
    }
    
    // Return -1: Nessun figlio rimasto.
    return 1; // TRUE: Puoi uscire
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

            // Evitiamo invii vuoti che potrebbero confondere il server
            if (strlen(buffer) == 0) continue;

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

            int background_mode = 0;
            char *local_path = NULL;
            char *remote_path = NULL;

            char *token = strtok(buffer, " \n"); // first token is "upload"

            token = strtok(NULL, " \n"); // second token is the local path

            if (token == NULL) {
                printf("Uso: upload [-b] <local_path> <server_path>\n");
                continue;
            }

            if (strcmp(token, "-b") == 0) {
                // Trovata opzione background!
                background_mode = 1;
                
                // Il prossimo token deve essere il local_path
                local_path = strtok(NULL, " \n");
            } else {
                // Non c'è -b, quindi questo token è già il local_path
                local_path = token;
            }


            if (local_path != NULL) {
                remote_path = strtok(NULL, " \n");
            }


            if (local_path == NULL || remote_path == NULL) {
                printf("Error: Missing arguments.\n");
                printf("Usage: upload [-b] <local_path> <server_path>\n");
                continue;
            }

            printf("[DEBUG] Local: %s | Remote: %s | Bg: %d\n", local_path, remote_path, background_mode);
            
            upload_file(sockfd, local_path, remote_path, background_mode, ip, port, current_username);

            //send_message(sockfd, "");
            continue;

        }

            if (strncmp(buffer, "download", 8) == 0) {

                int background_mode = 0;
                char *remote_path = NULL;
                char *local_path = NULL;

                char *token = strtok(buffer, " \n"); // first token is "download"

                token = strtok(NULL, " \n"); // second token is the remote path

                if (token == NULL) {
                    printf("Uso: download [-b] <server_path> <local_path>\n");
                    continue;
                }

                if (strcmp(token, "-b") == 0) {
                    // Trovata opzione background!
                    background_mode = 1;
                    
                    // Il prossimo token deve essere il remote_path
                    remote_path = strtok(NULL, " \n");
                } else {
                    // Non c'è -b, quindi questo token è già il remote_path
                    remote_path = token;
                }

                if (remote_path != NULL) {
                    local_path = strtok(NULL, " \n");
                }


                if (local_path == NULL || remote_path == NULL) {
                    printf("Error: Missing arguments.\n");
                    printf("Usage: download [-b] <server_path> <local_path>\n");
                    continue;
                }

                printf("[DEBUG] Remote: %s | Local: %s | Bg: %d\n", remote_path, local_path, background_mode);

                download_file(sockfd, remote_path, local_path, background_mode, ip, port, current_username);
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
            // If so, I enter "Send File" mode.
            // I do NOT have to immediately go back to the beginning of the while loop to do receive_message!
            send_message(sockfd, buffer);
            client_read_data(sockfd,buffer); 
            continue;
            }
            
            send_message(sockfd, buffer);
        }


        // memset(buffer, 0, sizeof(buffer)); 
        // int total_read = 0;

        
        // int n= receive_message(sockfd, buffer, sizeof(buffer)-1);
        // if(n <=0){
        //     printf("Connection closed by server\n");
        //     break;
        // }
                
        // printf("Size: %ld and Response from server:%s",strlen(buffer), buffer);
        // memset(buffer, 0, sizeof(buffer));
        
        //send_message(sockfd, buffer);
        
        
        // fgets(buffer, sizeof(buffer), stdin);
        // buffer[strcspn(buffer, "\n")] = 0; //remove newline character
        


        
        // if (strncmp(buffer, "upload", 6) == 0) {

        //     int background_mode = 0;
        //     char *local_path = NULL;
        //     char *remote_path = NULL;

        //     char *token = strtok(buffer, " \n"); // first token is "upload"

        //     token = strtok(NULL, " \n"); // second token is the local path

        //     if (token == NULL) {
        //         printf("Uso: upload [-b] <local_path> <server_path>\n");
        //         continue;
        //     }

        //     if (strcmp(token, "-b") == 0) {
        //         // Trovata opzione background!
        //         background_mode = 1;
                
        //         // Il prossimo token deve essere il local_path
        //         local_path = strtok(NULL, " \n");
        //     } else {
        //         // Non c'è -b, quindi questo token è già il local_path
        //         local_path = token;
        //     }


        //     if (local_path != NULL) {
        //         remote_path = strtok(NULL, " \n");
        //     }


        //     if (local_path == NULL || remote_path == NULL) {
        //         printf("Error: Missing arguments.\n");
        //         printf("Usage: upload [-b] <local_path> <server_path>\n");
        //         continue;
        //     }

        //     printf("[DEBUG] Local: %s | Remote: %s | Bg: %d\n", local_path, remote_path, background_mode);
            
        //     upload_file(sockfd, local_path, remote_path, background_mode);

        //     send_message(sockfd, "");
        //     continue;

        // }
        
        // send_message(sockfd, buffer);
        
        // if(strcmp(buffer, "exit") == 0){
        //     break;
        // }
        
    }
    

    

    close(sockfd);
    return 0;
}