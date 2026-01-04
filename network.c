#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>



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
     int total = 0;

    if (size <= 0) return -1;

    while (total < size - 1) {
        char c;
        ssize_t n = read(sockfd, &c, 1);

        if (n < 0) {
            perror("Read failed");
            return -1;
        }
        if (n == 0) {
            // connection closed
            return 0;
        }

        buffer[total++] = c;

        if (c == '\0') {
            // end of message
            return total;
        }
    }

    //the message is too long: truncate but close properly
    buffer[size - 1] = '\0';
    return total;
}