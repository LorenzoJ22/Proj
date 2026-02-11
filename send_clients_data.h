#ifndef COMMANDS_CLIENT_H
#define COMMANDS_CLIENT_H

#include "network.h"

void client_write_data(int sockfd);
void client_read_data(int sockfd);
void client_upload(int sockfd, char* buffer, char* ip, int port, char* current_username);
void client_download(int sockfd, char* buffer, char* ip, int port, char* current_username);

#endif