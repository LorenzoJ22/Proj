#ifndef COMMANDS_CLIENT_H
#define COMMANDS_CLIENT_H

#include "session.h"


void login(char *buffer, int client_fd, Session *s, SharedMemory *shm);
void create_user(int client_fd, char *buffer, Session *s);

void create(int client_fd, char *buffer, Session *s);
void change_directory(int client_fd, char *buffer, Session *s);
void chmods(int client_fd, char *buffer, Session *s);
void move(int client_fd, char *buffer, Session *s);
void list(int client_fd, char *buffer, Session *s);
void delete(int client_fd, char* buffer, Session *s);
void write_client(int client_fd, char* buffer, Session *s);
void read_client(int client_fd, char *buffer, Session *s);
void upload(int client_fd, char *buffer, Session *s);
void download(int client_fd, char *buffer, Session *s);
void transfer_request(int client_fd, char *buffer, SharedMemory *shm, Session *s);
int notify_transfer_requests(int client_fd, SharedMemory *shm, Session *s);
void accept_transfer_request(int client_fd, char *buffer, SharedMemory *shm, Session *s);
#endif