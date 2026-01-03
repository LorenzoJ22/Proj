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
        
        
        
        send_message(sockfd, buffer);
        
        if(strcmp(buffer, "exit") == 0){
            break;
        }
        
    }
    

    

    close(sockfd);
    return 0;
}