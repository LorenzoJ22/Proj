#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "session.h"

void handle_client(int client_fd, const char *root_dir);
void send_prompt(int client_fd, Session *s);

#endif