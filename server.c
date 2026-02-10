#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>


#include "network.h"
#include "client_handler.h"
#include "permissions.h"
#include "system_ops.h"
#include "values.h"
#include "shared.h"









int main (int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <root_directory> [ip] [port]\n", argv[0]);
        exit(1);
    }

    fd_set readfds; // La lista degli "occhi" del server
    int max_fd;

    // store command line arguments and print them
    char *root_dir = argv[1];
    char *ip = (argc >= 3) ? argv[2] : "127.0.0.1";
    int port = (argc >= 4) ? atoi(argv[3]) : 8080;

    printf("Root directory: %s\n", root_dir);
    printf("IP: %s\n", ip);
    printf("Port: %d\n", port);

    create_group(GROUP_NAME);

    // Check if root directory exists, if not create it
    sys_make_directory(root_dir, 0770, GROUP_NAME, "root");




    int server_fd = create_server_socket(ip, port);
    printf("Server listening on %s:%d\n", ip, port);


    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // Reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Restart interrupted system calls

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    //int incoming_request = 0;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Create shared memory
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedMemory), IPC_CREAT |IPC_EXCL| 0666);
    
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }


    SharedMemory *shm = (SharedMemory *)shmat(shm_id, NULL, 0);
    if (shm == (SharedMemory *) -1) {
        perror("shmat");
        exit(1);
    }

    memset(shm, 0, sizeof(SharedMemory));
    shm->global_id_counter = 1;

     if(sem_init(&shm->semaphore, 1, 1) == -1) {
        perror("sem_init");
        exit(1);
    }

    int r1;
    int r2;

    while(1){

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        max_fd = (server_fd > STDIN_FILENO) ? server_fd : STDIN_FILENO;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("Select error"); // If select returns -1 and it's not because of an interrupted system call, print error
            break; 
        }

        if (activity < 0 && errno == EINTR) {
            continue; 
        }



        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            // Handle stdin input
            char buffer[1024];
            fgets(buffer, sizeof(buffer), stdin);
            printf("Received input: %s", buffer);

                if (strcmp(buffer, "exit\n") == 0) {
                    printf(COLOR_RED"Shutting down server...\n"COLOR_RESET);
                    close(server_fd);
                    

                    sem_destroy(&shm->semaphore);
                    r1= shmdt(shm);

                     if (r1 == -1) {
                            perror("shmdt");
                        } else {
                            printf("Shared memory detached successfully.\n");
                    }

                    r2 = shmctl(shm_id, IPC_RMID, NULL);

                    if (r2 == -1) {
                        perror("shmctl");
                    } else {
                        printf("Shared memory marked for deletion successfully.\n");
                    }

                    kill(0, SIGTERM);

                    return 0;
                }else {
                    printf(COLOR_YELLOW"Unknown command, enter 'exit' to quit"COLOR_RESET);
                }
        }


        if(FD_ISSET(server_fd, &readfds)) {

            int client_fd = accept_connection(server_fd);
            printf("Client connected\n");

            if (client_fd < 0) {
                perror("Accept failed");
                continue;
            }

            pid_t pid = fork();

            if (pid < 0) {
                perror("Fork failed");
                close(client_fd);
                continue;
            }

            if (pid == 0) {
                // Child process
                close(server_fd); // Close the listening socket in child
                handle_client(client_fd, root_dir, shm);
                exit(0);
            }

            close(client_fd); // Close the connected socket in parent

        }

 
    }

    

    

    

    sem_destroy(&shm->semaphore);
    r1 =shmdt(shm);
     if (r1 == -1) {
        perror("shmdt");
    } else {
        printf("Shared memory detached successfully.\n");
    }
    r2 =shmctl(shm_id, IPC_RMID, NULL);

        if (r2 == -1) {
            perror("shmctl");
        } else {
            printf("Shared memory marked for deletion successfully.\n");
        }
   

    
    close(server_fd);
    return 0;

    

}