#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "network.h"

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]){

    
    char buffer[BUFFER_SIZE];  

    //if not enough arguments, use default values
    char *ip = (argc >= 3) ? argv[2] : "127.0.0.1";
    int port = (argc >= 4) ? atoi(argv[3]) : 8080;


    int sockfd = connect_to_server(ip, port);
    printf("Connesso a %s:%d\n", ip, port);


    while(1){

        memset(buffer, 0, sizeof(buffer)); 
        int n= receive_message(sockfd, buffer, sizeof(buffer)-1);
        if(n <=0){
            printf("Connection closed by server\n");
            break;
        }
        //buffer[n] = '\0';        
        printf("Size: %ld and Response from server:%s",strlen(buffer), buffer);
        memset(buffer, 0, sizeof(buffer));
        //printf("Insert command to send to server: ");
        //fflush(stdout);
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0; //remove newline character
        //^i try to move the remover of \n beacuse he shift the output..
        
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
            
            upload_file(sockfd, local_path, remote_path, background_mode);

            continue;

        }
        
        send_message(sockfd, buffer);
        
        if(strcmp(buffer, "exit") == 0){
            break;
        }
        
    }
    

    

    close(sockfd);
    return 0;
}