#include "shared.h"
#include "values.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

void register_user(SharedMemory *shm, const char *username) {

    sem_wait(&shm->semaphore);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shm->users[i].is_online == 0) { //if we found a spot that is free, we register the user there

            strncpy(shm->users[i].username, username, sizeof(shm->users[i].username)); // copy username to shared memory

            shm->users[i].pid = getpid(); // store the PID of the process handling this user

            shm->users[i].is_online = 1; // mark this slot as occupied

            printf(COLOR_CIANO "[DEBUG] Registered user '%s' with PID %d at slot %d\n" COLOR_RESET, username, shm->users[i].pid, i);


            printf("\n--- STATO SHARED MEMORY (Visto dal PID %d) ---\n", getpid());
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (shm->users[j].is_online) {
                        printf("[%d] Username: %s | PID: %d\n", 
                            j, shm->users[j].username, shm->users[j].pid);
                    }
                }
            printf("----------------------------------------------\n");
            
            

            sem_post(&shm->semaphore);
            return;
        }
    }
    
     printf(COLOR_RED "[ERROR] No space to register user '%s' in shared memory!\n" COLOR_RESET, username);
        sem_post(&shm->semaphore);
}

void unregister_user(SharedMemory *shm, const char *username) {
    sem_wait(&shm->semaphore);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shm->users[i].is_online == 1 && strcmp(shm->users[i].username, username) == 0) {
            shm->users[i].is_online = 0;
            memset(shm->users[i].username, 0, sizeof(shm->users[i].username)); // clear username
            shm->users[i].pid = 0; // clear PID
            printf(COLOR_CIANO "[DEBUG] Unregistered user '%s' from slot %d\n" COLOR_RESET, username, i);
            sem_post(&shm->semaphore);
            return;
        }
    }
    sem_post(&shm->semaphore);
     
}
