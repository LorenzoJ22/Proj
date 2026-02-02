#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "values.h"
#include <sys/select.h>



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

int perform_upload_logic(int sockfd, const char *local_path, const char *remote_path){
    FILE *fp = fopen(local_path, "rb");
        if (fp == NULL ) {
            perror("File open error");
            return -1;
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
            return -1;
        }

        printf("Server is ready. Sending %ld bytes...\n", filesize);

        char buffer[BUFFER_SIZE];
        size_t bytes_sent = 0;

        while (bytes_sent < (size_t)filesize) {
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
            printf(COLOR_GREEN"File uploaded successfully.\n"COLOR_RESET);
        } else {
            printf("Errore dal server: %s\n", ack);
        }
    }

    fclose(fp);
    return 0;
}

void upload_file (int sockfd, const char *local_path, const char *remote_path, int background_mode, char *server_ip, int server_port, char *username) {

    

    if (background_mode) {
        printf("Uploading file in background mode...\n");
        
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            return;
        }

        if (pid > 0) {
            // Parent process
            printf("Upload started in background with PID %d\n", pid);
            return;
        }

        if (pid == 0) {
            // Child process
            int bg_sockfd = connect_to_server(server_ip, server_port);
            printf(COLOR_YELLOW"[BG] Connesso. Inizio trasferimento...\n"COLOR_RESET);


            if (strlen(username) > 0) {

                char login_cmd[128];
                sprintf(login_cmd, "login %s", username); // Ricostruisce il comando

                printf(COLOR_YELLOW"[BG] Eseguo auto-login come '%s'...\n"COLOR_RESET, username);

                send(bg_sockfd, login_cmd, strlen(login_cmd), 0);

                char temp_buf[1024];
                while(1) {
                    fd_set fds;
                    struct timeval tv;
                    
                    FD_ZERO(&fds);
                    FD_SET(bg_sockfd, &fds);

                    // Timeout breve (0.1 secondi): se il server sta zitto per 0.1s, assumiamo abbia finito
                    tv.tv_sec = 0;
                    tv.tv_usec = 100000;

                    int ready = select(bg_sockfd + 1, &fds, NULL, NULL, &tv);
                    
                    if (ready <= 0) break; // Timeout (silenzio) o errore -> usciamo
                    
                    // Leggiamo e buttiamo via (non stampiamo per non sporcare il terminale del padre)
                    recv(bg_sockfd, temp_buf, sizeof(temp_buf), 0);
                }
                
            }

            int result = perform_upload_logic(bg_sockfd, local_path, remote_path);

            if (result == 0) {
                printf(COLOR_GREEN"[Background] Command: upload %s %s concluded\n"COLOR_RESET, remote_path, local_path);
            } else {
                printf(COLOR_RED"[Background] Command: upload %s %s failed with code %d.\n"COLOR_RESET, remote_path, local_path, result);
            }

            
            
            close(bg_sockfd);
            exit(0);

        }
    }

    perform_upload_logic(sockfd, local_path, remote_path);
    
}


int perform_download_logic(int sockfd, const char *remote_path, const char *local_path){

    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "download %s", remote_path);

    send_message(sockfd, command);

    char server_reply[BUFFER_SIZE] = {0};
    receive_message (sockfd, server_reply, sizeof(server_reply));

    if (strncmp(server_reply, "SIZE ", 5) != 0) {

        fprintf(stderr, "Server denied download: %s\n", server_reply);
        return -1;
    }

    long filesize = atol(server_reply + 5);
    printf("[INFO] File size: %ld bytes. Starting download...\n", filesize);

    send_message(sockfd, "READY");

    FILE *fp = fopen(local_path, "wb");

    if (fp == NULL ) {
        perror("File creation error");
        return -1;
    }

    long total_received = 0;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (total_received < filesize) {
        long bytes_left = filesize - total_received;


        int bytes_to_read = (bytes_left < (long)sizeof(buffer)) ? bytes_left : (long)sizeof(buffer);

        bytes_received = recv(sockfd, buffer, bytes_to_read, 0);
        if (bytes_received <= 0) {
            perror("Receive error"); 
            printf(COLOR_RED"\n[Client] Download interrupted.\n"COLOR_RESET);
            break;
        }

        fwrite(buffer, 1, bytes_received, fp);
        
        
        total_received += bytes_received;
        
        
        printf("\r[Client] Ricevuti: %ld / %ld bytes...", total_received, filesize);
        
        
         
    }

    fflush(fp);
    fclose(fp);

    

    char *msg = "SUCCESS";
    send(sockfd, msg, strlen(msg), 0);

    printf("\nDownload completed. Sending confirmation to server...\n");


    printf(COLOR_GREEN"File downloaded successfully: %s\n"COLOR_RESET, local_path);
    return 0;
}

void download_file(int sockfd, const char *remote_path, const char *local_path, int background_mode, char *server_ip, int server_port, char *username) {

    if (background_mode) {
        printf("Downloading file in background mode...\n");
        
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            return;
        }

        if (pid > 0) {
            // Parent process
            printf("Download started in background with PID %d\n", pid);
            return;
        }

        if (pid == 0) {

            // Child process
            int bg_sockfd = connect_to_server(server_ip, server_port);
            printf(COLOR_YELLOW"[BG] Connesso. Inizio trasferimento...\n"COLOR_RESET);
            
            if (strlen(username) > 0) {

                char login_cmd[128];
                sprintf(login_cmd, "login %s", username); // Ricostruisce il comando

                printf(COLOR_YELLOW"[BG] Eseguo auto-login come '%s'...\n"COLOR_RESET, username);

                send(bg_sockfd, login_cmd, strlen(login_cmd), 0);

                char temp_buf[1024];
                while(1) {
                    fd_set fds;
                    struct timeval tv;
                    
                    FD_ZERO(&fds);
                    FD_SET(bg_sockfd, &fds);

                    // Timeout breve (0.1 secondi): se il server sta zitto per 0.1s, assumiamo abbia finito
                    tv.tv_sec = 0;
                    tv.tv_usec = 100000;

                    int ready = select(bg_sockfd + 1, &fds, NULL, NULL, &tv);
                    
                    if (ready <= 0) break; // Timeout (silenzio) o errore -> usciamo
                    
                    // Leggiamo e buttiamo via (non stampiamo per non sporcare il terminale del padre)
                    recv(bg_sockfd, temp_buf, sizeof(temp_buf), 0);
                }
                
            }

            int result = perform_download_logic(bg_sockfd, remote_path, local_path);

            if (result == 0) {
                printf(COLOR_GREEN"[Background] Command: download %s %s concluded\n"COLOR_RESET, remote_path, local_path);
            } else {
                printf(COLOR_RED"[Background] Command: download %s %s failed with code %d.\n"COLOR_RESET, remote_path, local_path, result);
            }

            
            
            close(bg_sockfd);
            exit(0);
        }
    }

    perform_download_logic(sockfd, remote_path, local_path);
}