#ifndef SHARED_H
#define SHARED_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#define MAX_CLIENTS 10
#define MAX_REQUESTS 20
#define SHM_KEY 9999 // Chiave univoca

typedef struct {
    char username[32];
    pid_t pid;       // Il PID del processo figlio che gestisce questo utente
    char home_dir[128]; // Utile per sapere dove cercare i file
    int is_online;   // 1 = online, 0 = slot libero
} UserEntry;

typedef struct {
    int id;                // ID generato dal sistema
    char sender[32];       // Chi manda
    char receiver[32];     // Chi riceve
    char filename[128];    // Il file (che si trova nella home del sender)
    int status;            // 0=Vuoto, 1=In Attesa (Pending)
} TransferRequest;

typedef struct {
    UserEntry users[MAX_CLIENTS];
    TransferRequest requests[MAX_REQUESTS];
    int global_id_counter; 
} SharedMemory;


void register_user(SharedMemory *shm, const char *username);
void unregister_user(SharedMemory *shm, const char *username);

#endif