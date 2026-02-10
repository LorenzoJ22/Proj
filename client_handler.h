#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "session.h"
#include "shared.h"

extern int incoming_request;

void handle_client(int client_fd, const char *root_dir, SharedMemory *shm);
void send_prompt(int client_fd, Session *s);
void sigchld_handler();
void signal_handler(int signum);

#endif