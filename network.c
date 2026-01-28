#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "values.h"



struct sockaddr_in make_address(const char *ip, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET; // IPv4
    addr.sin_port = htons(port); // Port
    inet_pton(AF_INET, ip, &addr.sin_addr); // IP

    return addr;
}


// Function to create, bind, and listen on a server socket, returning the server file descriptor
int create_server_socket(const char *ip, int port) {
    

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr = make_address(ip, port);


    /*memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(port); // Port
    inet_pton(AF_INET, ip, &server_addr.sin_addr); // IP*/
    


    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(1);
    }


    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(1);
    }

    return server_fd;

}


// Function to accept a connection on the server socket, returning the client file descriptor

int accept_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("Accept failed");
        exit(1);
    }

    return client_fd;
}



int connect_to_server(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    } puts("Socket created successfully");

    struct sockaddr_in server_addr = make_address(ip, port);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(1);
    }

    return sockfd;
}


int send_message(int sockfd, const char *msg) {
    return write(sockfd, msg, strlen(msg) + 1);
}


int receive_message(int sockfd, char *buffer, int size) {
    return read(sockfd, buffer, size);
}

void upload_file (int sockfd, const char *local_path, const char *remote_path, int background_mode){

    if (background_mode) {
        printf("Uploading file in background mode...\n");
        // Implement background upload logic if needed
    }

    FILE *fp = fopen(local_path, "rb");
    if (fp == NULL ) {
        perror("File open error");
        return;
    }

    fseek (fp, 0, SEEK_END);
    long filesize = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "upload %s %ld", remote_path, filesize);

    send_message(sockfd, command);

    char server_reply[BUFFER_SIZE] = {0};
    receive_message (sockfd, server_reply, sizeof(server_reply));

    if (strncmp(server_reply, "READY", 5) != 0) {

        fprintf(stderr, "Server denied upload: %s\n", server_reply);
        fclose(fp);
        return;
    }

    printf("Server is ready. Sending %ld bytes...\n", filesize);

    char buffer[BUFFER_SIZE];
    size_t bytes_sent = 0;

    while (bytes_sent < filesize) {
        size_t to_send = fread(buffer, 1, sizeof(buffer), fp);
        if (to_send > 0) {
            send_message(sockfd, buffer);
            bytes_sent += to_send;
            printf("\r[Client] Inviati: %ld byte...", bytes_sent);
        }
    }

    printf("File inviato. Attendo conferma dal server...\n");

char ack[64] = {0};
// Questa recv blocca il client finché il server non ha finito VERAMENTE di leggere
if (recv(sockfd, ack, sizeof(ack), 0) <= 0) {
    perror("Errore ricezione conferma");
} else {
    if (strncmp(ack, "SUCCESS", 7) == 0) {
        printf("Server ha confermato: File salvato correttamente.\n");
    } else {
        printf("Errore dal server: %s\n", ack);
    }
}

fclose(fp);
}