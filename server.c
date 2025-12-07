#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "network.h"



int main (int argc, char *argv[]) {



    if (argc < 2) {
        fprintf(stderr, "Usage: %s <root_directory> [ip] [port]\n", argv[0]);
        exit(1);
    }

    // store command line arguments and print them
    char *root_dir = argv[1];
    char *ip = (argc >= 3) ? argv[2] : "127.0.0.1";
    int port = (argc >= 4) ? atoi(argv[3]) : 8080;

    printf("Root directory: %s\n", root_dir);
    printf("IP: %s\n", ip);
    printf("Port: %d\n", port);

    

    // Check if root directory exists, if not create it
 /*   struct stat st;
    if (stat(root_dir, &st) == -1) {
        printf("Root directory does not exist. Creating it...\n");
        if (mkdir(root_dir, 0700) < 0) {
            perror("Failed to create root directory");
            exit(1);
        }
    }


*/



    int server_fd = create_server_socket(ip, port);
    printf("Server listening on %s:%d\n", ip, port);

    //while(1){
        int client_fd = accept_connection(server_fd);
        printf("Client connected\n");


        


        //add here code to fork and handle multiple clients concurrently
        close(server_fd);
    //}

    

    close(server_fd);
    return 0;




}