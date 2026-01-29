#ifndef NETWORK_H
#define NETWORK_H

struct sockaddr_in make_address(const char *ip, int port);

int create_server_socket(const char *ip, int port); //create and bind server socket TCP listening on ip:port

int accept_connection(int server_fd);

int connect_to_server(const char *ip, int port); //create and connect client socket TCP to ip:port

int send_message(int sockfd, const char *message);

int receive_message(int sockfd, char *buffer, int size);

void upload_file(int sockfd, const char *local_path, const char *remote_path, int background_mode, char *server_ip, int server_port, char *username);

#endif