#ifndef COMMANDS_CLIENT_H
#define COMMANDS_CLIENT_H

#include "session.h"


void login(char *buffer, int client_fd, Session *s);
void create_user(int client_fd, char *buffer, Session *s);

void create(int client_fd, char *buffer, Session *s);
void change_directory(int client_fd, char *buffer, Session *s);
void chmods(int client_fd, char *buffer, Session *s);
void move(int client_fd, char *buffer, Session *s);
void list(int client_fd, char *buffer, Session *s);
#endif