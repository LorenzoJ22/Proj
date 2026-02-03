#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "network.h"
#include "values.h"

#define BUFFER_SIZE 2048


void client_write_data(int sockfd) {
    char file_buf[1024];

    printf("--- INIZIO SCRITTURA FILE ---\n");
    printf("DA CLIENT: Scrivi il testo. Dopodiche' scrivi 'END' su una nuova riga isolato e premi invio per terminare.\n");

    
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
    
    printf("---DA CLIENT: FINE SCRITTURA. Attendo conferma salvataggio... ---\n");
    // Appena esco da qui, il while principale riprende e...
    // ...incontrerà receive_message() che leggerà "File salvato correttamente".
}



void client_read_data(int sockfd,char *buffer) {
    char file_buf[BUFFER_SIZE];

    // int is_set = 0; 
    // char *args = buffer + 5;
    // int num=0;
    // int consumed=0;
    
    // if (strncmp(args, "-offset ", 8) == 0) {
    //     is_set = 1;      // We found the option!
    //     args += 8;       // Shift the pointer of three positions (hop " ")
    //     int i;
    //     if( (i=sscanf(args, "%d%n", &num, &consumed))==1){//%*[^0-9]%d  ---> hop all the non integer input , %n per contare
    //     args += consumed;
    //     // Now args point to the beginnig of the path
    // }else{
    //     printf("Fail to save num\n");
    // }
    // }

    // if(is_set && num > 1000){
    //     printf(COLOR_RED "Errore: Offset non valido\n" COLOR_RESET);
    //     return;}

    char path[64];
    if (sscanf(buffer+5, "%63s", path) != 1) {
        printf("Usage: read -offset=<num> <path>\n");
        return;
    }
    //write_client(sockfd, buffer);
   
    
    memset(file_buf, 0, BUFFER_SIZE);
    ssize_t l = read(sockfd, file_buf, BUFFER_SIZE-1);
    //printf("Letto da client: %ld\n", l);
    if(l<=0){return;}
    file_buf[l] = '\0'; // Assicura che la stringa sia terminata
    // 2. Controllo degli errori basato sul messaggio del server
    //printf("Letto dal client: %s\n", file_buf);
    // if (strstr(file_buf, "ERR_OFFSET") != NULL || strstr(file_buf, "ERR_OFFSET\n") != NULL) {
    //     memset(file_buf, 0, BUFFER_SIZE);
    //     printf(COLOR_RED "Errore: Offset non valido (più grande della dimensione del file)!\n" COLOR_RESET);
    //     return;
    // }
     if (strncmp(file_buf, "ERR_OFFSET", 10) == 0) {
        memset(file_buf, 0, BUFFER_SIZE);
        printf(COLOR_RED "Errore: Offset non valido (più grande della dimensione del file)!\n" COLOR_RESET);
        return;
    }
    // Leggo da tastiera
    printf("--- INIZIO TRASCRIZIONE FILE ---\n");
    printf("DA CLIENT: THIS IS THE TEXT OF THE FILE.\n");
    if (fputs(file_buf, stdout) == EOF){
        printf("Read to stdout failed.\n");
        return;
    }
    //write(sockfd, file_buf, strlen(file_buf));
    //printf("Ecco cosa hai scritto = %s",file_buf);
    printf("---DA CLIENT: FINE FILE... ---\n");
    
}