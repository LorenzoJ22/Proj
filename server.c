#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>


#include "network.h"
#include "client_handler.h"
#include "permissions.h"
#include "system_ops.h"
#include "values.h"






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

    create_group(GROUP_NAME);




    

    // Check if root directory exists, if not create it
    sys_make_directory(root_dir, 0770, GROUP_NAME);






    int server_fd = create_server_socket(ip, port);
    printf("Server listening on %s:%d\n", ip, port);

    while(1){
        int client_fd = accept_connection(server_fd);
        printf("Client connected\n");


        if (client_fd < 0) continue; //if accept failed, continue to next iteration

        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            close(client_fd);
            continue;
        }
        
        
        if (pid == 0) {
            // Child process
            close(server_fd); // Close the listening socket in child
            handle_client(client_fd, root_dir);
            exit(0);
        } 
            

         close(client_fd); // Close the connected socket in parent
        

        
    }

    

    close(server_fd);
    return 0;




}