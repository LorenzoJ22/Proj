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

        printf("Insert command to send to server: ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0; //remove newline character

        if(strcmp(buffer, "exit") == 0){
            break;
        }

        send_message(sockfd, buffer);

        memset(buffer, 0, sizeof(buffer));

        int n= receive_message(sockfd, buffer, sizeof(buffer));
        if(n <=0){
            printf("Connection closed by server\n");
            break;
        }

        printf("Response from server: %s\n", buffer);
    }
    

    

    close(sockfd);
    return 0;
}