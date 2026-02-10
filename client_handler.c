#include "client_handler.h"
#include "session.h"
#include "system_ops.h"
#include "values.h"
#include "permissions.h"
#include "commands_client.h"
#include "network.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include "shared.h"

int incoming_request = 0;


void sigchld_handler() {

    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void signal_handler(int signum) {
    if(signum == SIGUSR1){
        incoming_request = 1;
    }
}



void send_prompt(int client_fd, Session *s) {
    char prompt[4190];
    
    if (s->logged_in) {
        // Formato: username@shell:/path/corrente$ 
        char cwd[PATH_MAX];
        snprintf(prompt, sizeof(prompt), "\033[1;32m%s@shell\033[0m:\033[1;34m%s\033[0m$ ", 
                 s->username, /* s->current_dir */ getcwd(cwd, PATH_MAX));
    } else {
        // Formato: guest:/path/corrente$
        snprintf(prompt, sizeof(prompt), "guest:%s$ ", s->current_dir);
    }
    
    write(client_fd, prompt, strlen(prompt));
}


void handle_client(int client_fd, const char *root_dir, SharedMemory *shm) {

    Session s;
    session_init(&s, root_dir);

    char buffer[1024];

 
    while (1) {

        memset(buffer, 0, sizeof(buffer)); // clear buffer

        

        if(incoming_request) {
           
            printf("[FIGLIO %d] Controllo richieste pendenti...\n", getpid());
            int id =notify_transfer_requests(client_fd, shm, &s);

            int g = read(client_fd, buffer, sizeof(buffer)-1);

            if (g <= 0) {
                unregister_user(shm, s.username);
                break; // connection closed or error
            }

            if(strncmp(buffer, "accept ", 7) == 0 && (incoming_request == 1)){
                accept_transfer_request(client_fd, buffer, shm, &s);
                incoming_request = 0;
                continue;
            } else if(strncmp(buffer, "reject", 6) == 0){
                    sem_wait(&shm->semaphore);
                    shm->requests[id].outcome = 2; // rejected
                    sem_post(&shm->semaphore);
                    char msg[] = "Transfer request rejected\n";
                    write(client_fd, msg, strlen(msg));
                    incoming_request = 0;
                    continue;
            }
            send_message(client_fd, "No valid response received for transfer request. Please respond with 'accept <directory> <request_id>' or 'reject'.\n");
            continue;
        }

        memset(buffer, 0, sizeof(buffer)); // clear buffer
        send_prompt(client_fd, &s);
        int n = read(client_fd, buffer, sizeof(buffer)-1);
        if (n <= 0) {
            unregister_user(shm, s.username);
            break; // connection closed or error
        }
        printf("[FIGLIO %d] Messaggio ricevuto: %s\n", getpid(), buffer);
        printf("[DEBUG] Buffer pulito: '%s' (lunghezza: %lu)\n", buffer, strlen(buffer));

         if (strlen(buffer) == 0) {
            dprintf(client_fd,"Void space: '%s'\n", buffer);
            char b[PATH_MAX];
            dprintf(client_fd, "Current directory: %s\n",getcwd(b,PATH_MAX));
            continue; 
        } 

        // login command
        if (strncmp(buffer, "login ", 6) == 0) {
            login(buffer, client_fd, &s, shm);
            continue;
        }
        

        // create_user command
        if (strncmp(buffer, "create_user ", 12) ==0){
            create_user(client_fd, buffer, &s);
            continue;
        }

        /*Add here the if to check if the current user is logged in, so we don't have to repetly put it in every function*/

        //create [-d] <path><permission> command, create a file 
        if(strncmp(buffer, "create ", 7) ==0){
            create(client_fd, buffer, &s);
            continue;
        }

        if((strncmp(buffer, "cd ", 3)==0) || strncmp(buffer, "cd", 2)==0){
            change_directory(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "chmod ", 6)==0){
            chmods(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "move ", 5)==0){
            move(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "list ", 5)==0 || strncmp(buffer, "list", 4)==0){
            list(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "delete ", 7)==0){
            delete(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "write ", 6)==0){
            write_client(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "read ", 5)==0){
            read_client(client_fd, buffer, &s);
            sleep(0.5);
          continue;
        }

        if (strncmp(buffer, "upload ", 7) == 0) {
            upload(client_fd, buffer, &s);
            continue;
        }

        if (strncmp(buffer, "download ", 9) == 0) {
            download(client_fd, buffer, &s);
            continue;
        }

        if(strncmp(buffer, "transfer_request ", 17)==0){
            transfer_request(client_fd, buffer, shm, &s);
            continue;
        }



        //if not logged in, only allow login command
        if (!s.logged_in) {
            char msg[] = "Login first or create user command\n";
            write(client_fd, msg, strlen(msg));
            continue;
        }

        
        // unknown command
        char msg[] = "Unknown command \n";
        write(client_fd, msg, strlen(msg));
        

        
    }

 

    close(client_fd);
}