#ifndef NETWORK_H
#define NETWORK_H

int create_server_socket(const char *ip, int port); //create and bind server socket TCP listening on ip:port

int accept_connection(int server_fd);

#endif