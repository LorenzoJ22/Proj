#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "network.h"

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]){

    
    char buffer[BUFFER_SIZE];
    char resp[BUFFER_SIZE];   
    char prompt[BUFFER_SIZE];

    //if not enough arguments, use default values
    char *ip = (argc >= 3) ? argv[2] : "127.0.0.1";
    int port = (argc >= 4) ? atoi(argv[3]) : 8080;


    int sockfd = connect_to_server(ip, port);
    printf("Connesso a %s:%d\n", ip, port);


    int n= receive_message(sockfd, prompt, sizeof(prompt));
        if(n <=0){
            printf("Connection closed by server\n");
            return 0;
        }



    while(1){

        printf("%s", prompt);
        //fflush(stdout);

        /*memset(buffer, 0, sizeof(buffer));
        int n= receive_message(sockfd, buffer, sizeof(buffer));
        if(n <=0){
            printf("Connection closed by server\n");
            break;
        }

        printf("%s", buffer);
        memset(buffer, 0, sizeof(buffer));
        //printf("Insert command to send to server: "); */


        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0; //remove newline character

        
        if(strcmp(buffer, "exit") == 0){
            break;
        }

        send_message(sockfd, buffer);



        memset(resp, 0, sizeof(resp));
        n = receive_message(sockfd, resp, sizeof(resp));
        if (n <= 0) {
            printf("Connection closed by server\n");
            break;
        }
        
        printf("%s", resp);

        // 6) Ricevi il prompt successivo (che vale per il prossimo giro)
        memset(prompt, 0, sizeof(prompt));
        //n = receive_message(sockfd, prompt, sizeof(prompt));
        if (n <= 0) {
            printf("Connection closed by server\n");
            break;
        }

        
    }
    

    close(sockfd);
    return 0;
}