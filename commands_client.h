#ifndef COMMANDS_CLIENT_H
#define COMMANDS_CLIENT_H

#include "session.h"


void login(char *buffer, int client_fd, Session *s);
void create_user(int client_fd, char *buffer, Session *s);
void create(int client_fd, char *buffer, Session *s);
//void cd(int client_fd, char *buffer, Session *s);
void change_directory(int client_fd, char *buffer, Session *s);
void chmods(int client_fd, char *buffer, Session *s);
void move(int client_fd, char *buffer, Session *s);
void list(int client_fd, char *buffer, Session *s);
int normalize_path_list(char *path, Session *s, int client_fd);
#endif