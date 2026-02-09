#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "network.h"
#include "values.h"

#define SIZE 6000

void client_write_data(int sockfd, char *buffer) {
    uint32_t net_status;
    char file_buf[1024];

    // Leggiamo direttamente l'intero dello stato
    ssize_t m = read(sockfd, &net_status, sizeof(uint32_t));
    if (m <= 0) {
        perror("Errore ricezione stato dal server");
        return;
    }

    uint32_t status = ntohl(net_status);

    // Gestione errori tramite switch
    switch (status) {
        case RESP_OK:
            printf(COLOR_GREEN "File ready to be written\n" COLOR_RESET);
            break;

        case RESP_ERR_LOCKED:
            printf(COLOR_RED "Error: The file is currently in use by another user.\n" COLOR_RESET);
            return;

        case RESP_ERR_PATH:
            printf(COLOR_RED "Error: Destination directory not found or access denied\n" COLOR_RESET);
            return;

        case RESP_ERR_INVALID_NAME:
            printf(COLOR_RED "Error: File name not allowed\n" COLOR_RESET);
            return;

        case RESP_ERR_OFFSET_UNDER:
            printf(COLOR_RED "Error: Offset too small!\n" COLOR_RESET);
            return;

        case RESP_ERR_OFFSET_LONG:
            printf(COLOR_RED "Error: offset length too long!\n" COLOR_RESET);
            return;

        case RESP_ERR_NO_OFFSET:
            printf(COLOR_RED "Error: no offset inserted\n" COLOR_RESET);
            return;

        case RESP_USAGE:
            printf(COLOR_YELLOW "Usage: write -offset=<num> <path>\n" COLOR_RESET);
            return;

        case RESP_ERR_OPEN:
            printf(COLOR_RED "Error: Is not possible to open the file.\n" COLOR_RESET);
            return;

        case RESP_ERR_VIO:
            printf(COLOR_RED "Error: violation of the home\n" COLOR_RESET);
            return;

        case RESP_ERR_GUEST:
            printf(COLOR_RED "Cannot use write command while you are guest\n" COLOR_RESET);
            return;

        case RESP_ERR_INVALID_FILE_N:
            printf(COLOR_RED "Error: Invalid file name (trailing slash?)\n" COLOR_RESET);
            return;

        case RESP_ERR_INVALID_DOT:
            printf(COLOR_RED "Error: Cannot name a file or directory '.' or '..'\n" COLOR_RESET);
            return;

        default:
            printf(COLOR_RED "Generic Error or unknown code: %u\n" COLOR_RESET, status);
            return;
    }

    // --- DA QUI IN POI PROCEDE LA SCRITTURA ---
    printf(COLOR_MAGENTA "--- START WRITING FILE -------------\n" COLOR_RESET);
    printf(COLOR_CIANO "FROM CLIENT: Write here text. Type 'END' to finish.\n" COLOR_RESET);

    while(1) {
        memset(file_buf, 0, sizeof(file_buf));
        if (fgets(file_buf, sizeof(file_buf), stdin) == NULL) break;

        if (strcmp(file_buf, "END\n") == 0) {
            send_message(sockfd, "END");
            break; 
        }
        write(sockfd, file_buf, strlen(file_buf));
    }
    
    printf(COLOR_CIANO "--- TERMINATED. Waiting for server... ---\n" COLOR_RESET);
}


void client_read_data(int sockfd, char *buffer) {
    char file_buf[SIZE];
    uint32_t net_count = 0;
    
    if (read(sockfd, &net_count, sizeof(uint32_t)) <= 0) {
        perror("Errore lettura dimensione");
        return;
    }

    uint32_t total_to_receive = ntohl(net_count);
    uint32_t total_received = 0;
    int is_first_chunk = 1;
    int header_printed = 0; // Flag per stampare il titolo una sola volta

    while (total_received < total_to_receive) {
        uint32_t to_read = (total_to_receive - total_received > SIZE - 1) 
                           ? SIZE - 1 
                           : total_to_receive - total_received;

        ssize_t l = read(sockfd, file_buf, to_read);
        
        if (l <= 0) {
            fprintf(stderr, "\nErrore: Connection closed. Received %u/%u\n", total_received, total_to_receive);
            break;
        }

        if (is_first_chunk) {
            // --- CONTROLLO ERRORI ---
            if (strncmp(file_buf, "ERR_OFFSET", 10) == 0) {
                printf(COLOR_RED "Errore: Offset non valido!\n" COLOR_RESET);
                return;
            }
            if(strncmp(file_buf, "NO_OFF", 6)==0){
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
            }else if(strncmp(file_buf, "ERR_OPEN", 8)==0){
            memset(file_buf, 0, SIZE);
            printf(COLOR_RED"Error opening file..\n"COLOR_RESET);
            return;
            }else if(strncmp(file_buf, "ERR_NOT_FOUND", 13)==0){
            memset(file_buf, 0, SIZE);
            printf(COLOR_RED"Path invalid or not found!\n"COLOR_RESET);
            return;   
            }else if(strncmp(file_buf, "ERROR_PARAM", 11)==0){
            memset(file_buf, 0, SIZE);
            printf(COLOR_YELLOW"Usage: read -offset=<num> <path>\n"COLOR_RESET);
            return;   
            }else if(strncmp(file_buf, "ERR_VIO", 7)==0){
            memset(file_buf, 0, SIZE);
            printf(COLOR_RED"Error: violation of the home\n"COLOR_RESET);
            return;   
            }else if(strncmp(file_buf, "Cannot read files while you are guest", 37) == 0) {
                printf(COLOR_RED "Error: Cannot read files while you are guest\n" COLOR_RESET);
                return;   
            }
            // SE SIAMO QUI, NON È UN ERRORE: Stampiamo l'intestazione
            printf("Ricezione di %u byte...\n", total_to_receive);
            printf(COLOR_MAGENTA"--- START FILE TRANSCRIPTION ---\n"COLOR_RESET);
            header_printed = 1; 
            
            is_first_chunk = 0;
        }

        fwrite(file_buf, 1, l, stdout);
        fflush(stdout);
        total_received += l;
    }

    // Stampa il fine file solo se abbiamo effettivamente iniziato a scrivere il file
    if (header_printed) {
        printf(COLOR_CIANO "\n--- FINE FILE (%u byte) ---\n" COLOR_RESET, total_received);
    }
}


void client_upload(int sockfd, char* buffer, char* ip, int port, char* current_username){
    int background_mode = 0;
    char *local_path = NULL;
    char *remote_path = NULL;

    char *token = strtok(buffer, " \n"); // first token is "upload"

    token = strtok(NULL, " \n"); // second token is the local path

    if (token == NULL) {
        printf("Uso: upload [-b] <local_path> <server_path>\n");
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
        return;
    }

    printf("[DEBUG] Remote: %s | Local: %s | Bg: %d\n", remote_path, local_path, background_mode);

    download_file(sockfd, remote_path, local_path, background_mode, ip, port, current_username);
}
  

