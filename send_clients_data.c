#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "network.h"

#define BUFFER_SIZE 1024


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