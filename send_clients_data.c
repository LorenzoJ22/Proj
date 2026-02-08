#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "network.h"
#include "values.h"

#define SIZE 6000


// void client_write_data(int sockfd, char *buffer) {
//     sleep(0.5);
//     char file_buf[1024];
    
    
//     char err[64];
//     char * data = err;
//     memset(err,0,sizeof(err));
//     ssize_t m = read(sockfd, err,sizeof(err));

//     printf("[DEBUG CLIENT] : '%s', byte %ld.\n", err, m);

//     if (m > 0 && strncmp(err, "OK",2)==0) {
//         printf(COLOR_GREEN "File ready to be written\n" COLOR_RESET);
//     }else{
//     if (m > 0 && strstr(data, "ERROR_LOCKED")) {
//         printf(COLOR_RED "Error: File locked by another user!\n" COLOR_RESET);
//         return;
//     }
//     if (m > 0 && strstr(err, "ER_PATH")) {
//         printf(COLOR_RED "Error: Destination directory not found or access denied \n" COLOR_RESET);
//         return;
//     }
//     if (m > 0 && strstr(err, "INVALID_NAME")) {
//         printf(COLOR_RED "Error: File name not allowed \n" COLOR_RESET);
//         return;
//     }
//     if (m > 0 && strstr(err, "OFF_UNDER")) {
//         printf(COLOR_RED "Error: Offset too small!\n" COLOR_RESET);
//         return;
//     }

//     if(strncmp(err,"ERROR",5)==0){//wrong parse of path
//         printf(COLOR_YELLOW"Usage: write -offset=<num> <path>\n"COLOR_RESET);
//         return;
//     }else if(strncmp(err, "OFF_ER",6)==0){//wrong offset length
//         printf(COLOR_RED"Error: offset length too long!\n"COLOR_RESET);
//         return;
//     }else if(strncmp(err,"NO_OFF", 6)==0){//wrong parse of offset
//         printf(COLOR_RED"Error: no offset inserted\n"COLOR_RESET);
//         return;
//     }else if(strncmp(err,"US",2)==0){//wrong parse of offset without space
//         printf(COLOR_YELLOW "Usage no space after off: write -offset=<num> <path>\n" COLOR_RESET);
//         return;
//     }else if (strncmp(err, "ERROR_LOCKED", 12) == 0) {//file already blocked
//         printf(COLOR_RED "Error: The file is currently in use by another user. Please try again later.\n" COLOR_RESET);
//         return;
//     }else if (strncmp(err, "ER_FIL", 6) == 0) {//file already blocked
//         printf(COLOR_RED "Error: Is not possible to open the file .\n" COLOR_RESET);
//         return;
//     }else if(strncmp(err, "ER_VIOL",7)==0){
//         printf(COLOR_RED"Error: violation of the home\n"COLOR_RESET);
//         return;
//     }else if(strncmp(err, "GUES_ERR",8)==0){
//         printf("Cannot use write command while you are guest\n");
//         return;
//     }
// }

//     printf(COLOR_MAGENTA"--- START WRITING FILE -------------\n"COLOR_RESET);
//     printf(COLOR_CIANO"FROM CLIENT: Write here your text. Then write 'END' on a new line and press ENTER to close\n"COLOR_RESET);

    
//     //write_client(sockfd, buffer);
//     while(1) {
//         memset(file_buf, 0, sizeof(file_buf));

//         // Leggo da tastiera
//         if (fgets(file_buf, sizeof(file_buf), stdin) == NULL) break;
//         //printf("Ecco cosa hai scritto = %s",file_buf);
//         // Controllo se l'utente vuole finire
//         // fgets include il \n, quindi cerco "END\n"
//         if (strcmp(file_buf, "END\n") == 0) {
//             // Invio il terminatore al server
//             printf("Invio messaggio end da client\n");
//             send_message(sockfd, "END");
//             break; // ESCO e torno al ciclo principale
//         }

//         // Invio la riga di testo al server
//         // NOTA: Qui uso write diretta o send_message, l'importante è inviare i byte
//         write(sockfd, file_buf, strlen(file_buf));
//     }
    
//     printf(COLOR_CIANO"---FROM CLIENT: TERMINATE TO WRITE. Waiting for file saving... ---------\n"COLOR_RESET);
//     // Appena esco da qui, il while principale riprende e...
//     // ...incontrerà receive_message() che leggerà "File salvato correttamente".
// }
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












// void client_read_data(int sockfd,char *buffer) {
//     char file_buf[SIZE];

//     char path[64];
//     if (sscanf(buffer+5, "%63s", path) != 1) {
//         printf(COLOR_YELLOW"Usage: read -offset=<num> <path>\n"COLOR_RESET);
//         return;
//     }
   
//     char *data_to_print = file_buf;
//     memset(file_buf, 0, SIZE);
    
//     long network_count =0;
//     ssize_t l;
//     if (read(sockfd, &network_count, sizeof(int)) <= 0) {
//         perror("Errore lettura count");
//         return;
//     }
//     printf("Net_count %ld\n",network_count);
//     int count = ntohl(network_count);
//     printf("Il server invierà %d byte\n", count);

//     if (count <= 0) {
//         // Gestisci errore o file vuoto qui
//         printf("EMPty?\n");
//         return;
//     }

//     // 2. Leggi il contenuto basandoti su 'count'
//     // Nota: Usiamo un ciclo per assicurarci di leggere TUTTI i byte promessi
//     int total_received = 0;
    
//     while (total_received < count) {
//         l = read(sockfd, file_buf + total_received, count - total_received);
//         printf(COLOR_MAGENTA"Ho preso dal socket %ld\n"COLOR_RESET,l);
//         if (l <= 0) break;
//         if (fputs(file_buf, stdout)){
//             printf("Read to stdout failed.\n");
//             return;
//         }
//         printf(COLOR_MAGENTA"Total received adesso e' : %d\n"COLOR_RESET,total_received);
//         total_received += l;
//         memset(file_buf, 0, SIZE);
//     }
// // l= read(sockfd, file_buf, count);

    
//     // while (total_received < count) {
//     //     l = read(sockfd, file_buf + total_received, count - total_received);
//     //     printf("Received total_rec %d\n",total_received);
//     //     if (l <= 0) break;
//     //     total_received += l;
//     // }
     
//     //ssize_t l= read(sockfd, file_buf, SIZE-1);
//     // l=read(sockfd, file_buf, l);
//     // //printf("Letto da client: %ld\n", l);
//     // if(l<=0){return;}
//     file_buf[l] = '\0'; // Assicura che la stringa sia terminata
//     // 2. Controllo degli errori basato sul messaggio del server
//      if (strncmp(file_buf, "ERR_OFFSET", 10) == 0) {
//         memset(file_buf, 0, SIZE);
//         printf(COLOR_RED "Error: Offset not valid (too big for the file size)!\n" COLOR_RESET);
//         return;
//     }else if(strncmp(file_buf, "NO_OFF", 6)==0){
//         memset(file_buf, 0, SIZE);
//         printf(COLOR_RED "Error: Offset not inserted!\n" COLOR_RESET);
//         return;
//     }else if(strncmp(file_buf,"US",2)==0){
//         memset(file_buf, 0, SIZE);
//         printf(COLOR_YELLOW "Usage: read -offset=<num> <path>\n" COLOR_RESET);
//         return;
//     }else if(strncmp(file_buf, "EMPTY_OR_READ_ERROR", 19)==0){
//         memset(file_buf, 0, SIZE);
//         printf(COLOR_YELLOW"File empty.."COLOR_RESET COLOR_RED" ..or error while reading\n"COLOR_RESET);
//         return;
//     }else if(strncmp(file_buf, "ERROR_LOCKED", 12)==0){
//         memset(file_buf, 0, SIZE);
//         printf(COLOR_RED"File already in use!\n"COLOR_RESET);
//         return;
//     }else if(strncmp(file_buf, "OK", 2)==0){
//         data_to_print += 2;
//         printf("We are ready to read\n");
//     }

//     // Leggo da tastiera
//     // printf(COLOR_MAGENTA"--- START FILE TRANSCRIPTION---\n"COLOR_RESET);
//     // printf(COLOR_CIANO"FROM CLIENT: THIS IS THE CONTENT OF THE FILE.\n"COLOR_RESET);
//     // if (fputs(data_to_print, stdout) == EOF){
//     //     printf("Read to stdout failed.\n");
//     //     return;
//     // }
  
//     // //sleep(10);
   
//     printf(COLOR_CIANO"---FROM CLIENT: FINISH TO SHOW FILE... ---\n"COLOR_RESET);
// }
  
// void client_read_data(int sockfd, char *buffer) {
//     char file_buf[SIZE]; // SIZE può essere 4096 o quello che preferisci
//     char path[64];
    
//     if (sscanf(buffer + 5, "%63s", path) != 1) {
//         printf(COLOR_YELLOW"Usage: read -offset=<num> <path>\n"COLOR_RESET);
//         return;
//     }

//     uint32_t network_count = 0;
//     if (read(sockfd, &network_count, sizeof(uint32_t)) <= 0) {
//         perror("Errore lettura count");
//         return;
//     }

//     int count = ntohl(network_count);
//     printf("Il server invierà %d byte\n", count);

//     if (count <= 0) return;

//     printf(COLOR_MAGENTA"--- START FILE TRANSCRIPTION ---\n"COLOR_RESET);

//     int total_received = 0;
//     while (total_received < count) {
//         // Leggiamo al massimo SIZE-1 per lasciare spazio allo zero terminale se serve
//         int da_leggere = (count - total_received > SIZE - 1) ? SIZE - 1 : count - total_received;
        
//         ssize_t l = read(sockfd, file_buf, da_leggere);
        
//         if (l <= 0) {
//             perror("Connessione interrotta durante la ricezione");
//             break;
//         }

//         // Aggiungiamo il terminatore per poter usare printf/fputs in sicurezza
//         file_buf[l] = '\0';
        
//         // Gestione messaggi di errore (solo sul primo blocco ricevuto)
//         if (total_received == 0) {
//             if (strncmp(file_buf, "ERR_OFFSET", 10) == 0) { 
//                  return; 
//                 }
//             if (strncmp(file_buf, "OK", 2) == 0) {
//                 // Se c'è "OK", stampiamo solo quello che viene dopo
//                 fputs(file_buf + 2, stdout);
//             } else {
//                 fputs(file_buf, stdout);
//             }
//         } else {
//             // Blocchi successivi: stampa tutto
//             fputs(file_buf, stdout);
//         }

//         total_received += l;
//     }
    
//     printf(COLOR_CIANO"\n--- FINISH TO SHOW FILE ---\n"COLOR_RESET);
// }







// void client_read_data(int sockfd, char *buffer) {
//     char file_buf[SIZE];
//     uint32_t net_count = 0;

//     // 1. Leggi il numero di byte totali
//     if (read(sockfd, &net_count, sizeof(uint32_t)) <= 0) {
//         perror("Errore lettura dimensione");
//         return;
//     }

//     uint32_t total_to_receive = ntohl(net_count);
//     uint32_t total_received = 0;
//     int is_first_chunk = 1;

//     printf("Ricezione di %u byte...\n", total_to_receive);
//     //Loop to receive
//     while (total_received < total_to_receive) {
//         // Calcola quanto leggere per non sforare il buffer
//         uint32_t to_read = (total_to_receive - total_received > SIZE - 1) 
//                            ? SIZE - 1 
//                            : total_to_receive - total_received;

//         ssize_t l = read(sockfd, file_buf, to_read);
        
//         if (l <= 0) {
//             fprintf(stderr, "\nErrore: Connection closed. Received %u/%u\n", total_received, total_to_receive);
//             break;
//         }

//         // 3. Gestione errori del server nel primo pezzo
//         if (is_first_chunk) {
//             if (strncmp(file_buf, "ERR_OFFSET", 10) == 0) {
//                 printf(COLOR_RED "Errore: Offset non valido!\n" COLOR_RESET);
//                 return;
//             }
//             if(strncmp(file_buf, "NO_OFF", 6)==0){
//             memset(file_buf, 0, SIZE);
//             printf(COLOR_RED "Error: Offset not inserted!\n" COLOR_RESET);
//             return;
//             }else if(strncmp(file_buf,"US",2)==0){
//             memset(file_buf, 0, SIZE);
//             printf(COLOR_YELLOW "Usage: read -offset=<num> <path>\n" COLOR_RESET);
//             return;
//             }else if(strncmp(file_buf, "EMPTY_OR_READ_ERROR", 19)==0){
//             memset(file_buf, 0, SIZE);
//             printf(COLOR_YELLOW"File empty.."COLOR_RESET COLOR_RED" ..or error while reading\n"COLOR_RESET);
//             return;
//             }else if(strncmp(file_buf, "ERROR_LOCKED", 12)==0){
//             memset(file_buf, 0, SIZE);
//             printf(COLOR_RED"File already in use!\n"COLOR_RESET);
//             return;
//             }else if(strncmp(file_buf, "ERR_OPEN", 8)==0){
//             memset(file_buf, 0, SIZE);
//             printf(COLOR_RED"Error opening file..\n"COLOR_RESET);
//             return;
//             }else if(strncmp(file_buf, "ERR_NOT_FOUND", 13)==0){
//             memset(file_buf, 0, SIZE);
//             printf(COLOR_RED"Path invalid or not found!\n"COLOR_RESET);
//             return;   
//             }else if(strncmp(file_buf, "ERROR_PARAM", 11)==0){
//             memset(file_buf, 0, SIZE);
//             printf(COLOR_YELLOW"Usage: read -offset=<num> <path>\n"COLOR_RESET);
//             return;   
//             }else if(strncmp(file_buf, "ERR_VIO", 7)==0){
//             memset(file_buf, 0, SIZE);
//             printf(COLOR_RED"Error: violation of the home\n"COLOR_RESET);
//             return;   
//             }
//             is_first_chunk = 0;
//         }

        
//         // Usiamo fwrite invece di fputs/printf perché fwrite non si ferma se trova uno '\0' 
//         // ed è perfetta per file di ogni tipo.
//         fwrite(file_buf, 1, l, stdout);
//         fflush(stdout); // Forza l'uscita a video immediata

//         total_received += l;
//     }

//     printf(COLOR_CIANO "\n--- FINE FILE (%u byte) ---\n" COLOR_RESET, total_received);
// }



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
            }else if(strncmp(file_buf, "ERR_VIO", 7) == 0) {
                printf(COLOR_RED "Error: violation of the home\n" COLOR_RESET);
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
  

