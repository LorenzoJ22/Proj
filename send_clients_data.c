#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "network.h"
#include "values.h"

#define SIZE 2048


void client_write_data(int sockfd, char *buffer) {
    sleep(0.5);
    char file_buf[1024];
    
    
    char err[64];
    char * data = err;
    memset(err,0,sizeof(err));
    ssize_t m = read(sockfd, err,sizeof(err));

    printf("[DEBUG CLIENT] : '%s', byte %ld.\n", err, m);

    if (m > 0 && strncmp(err, "OK",2)==0) {
        printf(COLOR_GREEN "File ready to be written\n" COLOR_RESET);
    }else{
    if (m > 0 && strstr(data, "ERROR_LOCKED")) {
        printf(COLOR_RED "Error: File locked by another user!\n" COLOR_RESET);
        return;
    }
    if (m > 0 && strstr(err, "ER_PATH")) {
        printf(COLOR_RED "Error: Destination directory not found or access denied \n" COLOR_RESET);
        return;
    }
    if (m > 0 && strstr(err, "INVALID_NAME")) {
        printf(COLOR_RED "Error: File name not allowed \n" COLOR_RESET);
        return;
    }
    if (m > 0 && strstr(err, "OFF_UNDER")) {
        printf(COLOR_RED "Error: Offset too small!\n" COLOR_RESET);
        return;
    }

    if(strncmp(err,"ERROR",5)==0){//wrong parse of path
        printf(COLOR_YELLOW"Usage: write -offset=<num> <path>\n"COLOR_RESET);
        return;
    }else if(strncmp(err, "OFF_ER",6)==0){//wrong offset length
        printf(COLOR_RED"Error: offset length too long!\n"COLOR_RESET);
        return;
    }else if(strncmp(err,"NO_OFF", 6)==0){//wrong parse of offset
        printf(COLOR_RED"Error: no offset inserted\n"COLOR_RESET);
        return;
    }else if(strncmp(err,"US",2)==0){//wrong parse of offset without space
        printf(COLOR_YELLOW "Usage no space after off: write -offset=<num> <path>\n" COLOR_RESET);
        return;
    }else if (strncmp(err, "ERROR_LOCKED", 12) == 0) {//file already blocked
        printf(COLOR_RED "Error: The file is currently in use by another user. Please try again later.\n" COLOR_RESET);
        return;
    }else if (strncmp(err, "ER_FIL", 6) == 0) {//file already blocked
        printf(COLOR_RED "Error: Is not possible to open the file .\n" COLOR_RESET);
        return;
    }//else if(strncmp(err, "OK",2)==0){
    //     memset(err,0,sizeof(err));
    //     printf("Perfect we can take input\n");
    // }
}

    printf(COLOR_MAGENTA"--- START WRITING FILE -------------\n"COLOR_RESET);
    printf(COLOR_CIANO"FROM CLIENT: Write here your text. Then write 'END' on a new line and press ENTER to close\n"COLOR_RESET);

    
    //write_client(sockfd, buffer);
    while(1) {
        memset(file_buf, 0, sizeof(file_buf));

        // Leggo da tastiera
        if (fgets(file_buf, sizeof(file_buf), stdin) == NULL) break;
        //printf("Ecco cosa hai scritto = %s",file_buf);
        // Controllo se l'utente vuole finire
        // fgets include il \n, quindi cerco "END\n"
        if (strcmp(file_buf, "END\n") == 0) {
            // Invio il terminatore al server
            printf("Invio messaggio end da client\n");
            send_message(sockfd, "END");
            break; // ESCO e torno al ciclo principale
        }

        // Invio la riga di testo al server
        // NOTA: Qui uso write diretta o send_message, l'importante è inviare i byte
        write(sockfd, file_buf, strlen(file_buf));
    }
    
    printf(COLOR_CIANO"---FROM CLIENT: TERMINATE TO WRITE. Waiting for file saving... ---------\n"COLOR_RESET);
    // Appena esco da qui, il while principale riprende e...
    // ...incontrerà receive_message() che leggerà "File salvato correttamente".
}

void client_read_data(int sockfd,char *buffer) {
    char file_buf[SIZE];

    char path[64];
    if (sscanf(buffer+5, "%63s", path) != 1) {
        printf(COLOR_YELLOW"Usage: read -offset=<num> <path>\n"COLOR_RESET);
        return;
    }
    //write_client(sockfd, buffer);
    // char err[64];
    // ssize_t g =read()
    char *data_to_print = file_buf;
    memset(file_buf, 0, SIZE);
    ssize_t l = read(sockfd, file_buf, SIZE-1);
    //printf("Letto da client: %ld\n", l);
    if(l<=0){return;}
    file_buf[l] = '\0'; // Assicura che la stringa sia terminata
    // 2. Controllo degli errori basato sul messaggio del server
     if (strncmp(file_buf, "ERR_OFFSET", 10) == 0) {
        memset(file_buf, 0, SIZE);
        printf(COLOR_RED "Error: Offset not valid (too big for the file size)!\n" COLOR_RESET);
        return;
    }else if(strncmp(file_buf, "NO_OFF", 6)==0){
        memset(file_buf, 0, SIZE);
        printf(COLOR_RED "Error: Offset not inserted!\n" COLOR_RESET);
        return;
    }else if(strncmp(file_buf,"US",2)==0){
        memset(file_buf, 0, SIZE);
        printf(COLOR_YELLOW "Usage: read -offset=<num> <path>\n" COLOR_RESET);
        return;
    }else if(strncmp(file_buf, "EMPTY_OR_READ_ERROR", 19)==0){
        memset(file_buf, 0, SIZE);
        printf(COLOR_YELLOW"File empty.."COLOR_RESET COLOR_RED" ..or error while reading\n"COLOR_RESET);
        return;
    }else if(strncmp(file_buf, "ERROR_LOCKED", 12)==0){
        memset(file_buf, 0, SIZE);
        printf(COLOR_RED"File already in use!\n"COLOR_RESET);
        return;
    }else if(strncmp(file_buf, "OK", 2)==0){
        data_to_print += 2;
        printf("We are ready to read\n");
    }

    // Leggo da tastiera
    printf(COLOR_MAGENTA"--- START FILE TRANSCRIPTION---\n"COLOR_RESET);
    printf(COLOR_CIANO"FROM CLIENT: THIS IS THE CONTENT OF THE FILE.\n"COLOR_RESET);
    if (fputs(data_to_print, stdout) == EOF){
        printf("Read to stdout failed.\n");
        return;
    }
  
    sleep(10);
   
    printf(COLOR_CIANO"---FROM CLIENT: FINISH TO SHOW FILE... ---\n"COLOR_RESET);
}
  


void client_upload(int sockfd, char* buffer, char* ip, int port, char* current_username){
    int background_mode = 0;
    char *local_path = NULL;
    char *remote_path = NULL;

    char *token = strtok(buffer, " \n"); // first token is "upload"

    token = strtok(NULL, " \n"); // second token is the local path

    if (token == NULL) {
        printf("Uso: upload [-b] <local_path> <server_path>\n");
        send_message(sockfd, "");
        return;
    }

    if (strcmp(token, "-b") == 0) {
        // Trovata opzione background!
        background_mode = 1;
                
        // Il prossimo token deve essere il local_path
        local_path = strtok(NULL, " \n");
    } else {
        // Non c'è -b, quindi questo token è già il local_path
        local_path = token;
    }


    if (local_path != NULL) {
        remote_path = strtok(NULL, " \n");
    }


    if (local_path == NULL || remote_path == NULL) {
        printf("Error: Missing arguments.\n");
        printf("Usage: upload [-b] <local_path> <server_path>\n");
        send_message(sockfd, "");
        return;
    }

    printf("[DEBUG] Local: %s | Remote: %s | Bg: %d\n", local_path, remote_path, background_mode);
            
    upload_file(sockfd, local_path, remote_path, background_mode, ip, port, current_username);
}

void client_download(int sockfd, char* buffer, char* ip, int port, char* current_username){

    int background_mode = 0;
                char *remote_path = NULL;
                char *local_path = NULL;

                char *token = strtok(buffer, " \n"); // first token is "download"

                token = strtok(NULL, " \n"); // second token is the remote path

                if (token == NULL) {
                    printf("Uso: download [-b] <server_path> <local_path>\n");
                    send_message(sockfd, "");
                    return;
                }

                if (strcmp(token, "-b") == 0) {
                    // Trovata opzione background!
                    background_mode = 1;
                    
                    // Il prossimo token deve essere il remote_path
                    remote_path = strtok(NULL, " \n");
                } else {
                    // Non c'è -b, quindi questo token è già il remote_path
                    remote_path = token;
                }

    if (remote_path != NULL) {
                    local_path = strtok(NULL, " \n");
                }


    if (local_path == NULL || remote_path == NULL) {
        printf("Error: Missing arguments.\n");
        printf("Usage: download [-b] <server_path> <local_path>\n");
        send_message(sockfd, "");
        return;
    }

    printf("[DEBUG] Remote: %s | Local: %s | Bg: %d\n", remote_path, local_path, background_mode);

    download_file(sockfd, remote_path, local_path, background_mode, ip, port, current_username);
}
  

