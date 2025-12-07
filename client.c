#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]){
    int sock;
    struct sockaddr_in server_addr;


    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed");
        exit(1);
    }
    puts("Socket created successfully");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Connection to server failed");
        exit(1);
    }
    puts("Connected to server successfully");

    char msg[] = "Ciao server, sono il client!";
    write(sock, msg, strlen(msg) + 1);

    // 5. Ricevo risposta
    char buffer[1024];
    read(sock, buffer, sizeof(buffer));
    printf("Risposta dal server: %s\n", buffer);

    close(sock);
    return 0;
}