/*
 * Description:
 * ------------
 * This program demonstrates the use of **pipes** for interprocess communication (IPC) in Linux.
 * A pipe is a unidirectional communication channel: data written to the write-end can be read
 * from the read-end. In this example, a single process writes multiple messages to a pipe
 * and then reads them back.
 *
 * Key concepts:
 * - pipe(): creates a pipe with two file descriptors: p[0] for reading, p[1] for writing.
 * - write(): writes data to the pipe.
 * - read(): reads data from the pipe.
 * - strcpy(): copies a string into a buffer.
 * - Null terminator (`\0`) handling:
 *      Each string copied with strcpy includes the terminating `\0`. Since the program writes
 *      `sizeof(inbuf)` bytes (16) to the pipe for each message, the null terminator is included
 *      and ensures that when reading back, the string is properly null-terminated for printf.
 */

#include <stdlib.h>     // exit
#include <unistd.h>     // pipe, read, write
#include <stdio.h>      // printf, perror
#include <string.h>     // strcpy

#define MSGSIZE  16

char *msg1 = "hello #1";
char *msg2 = "hello #2";
char *msg3 = "hello #3";

void main()
{  
    char inbuf[MSGSIZE];
    int p[2], j;

    /* Create a pipe: p[0] = read end, p[1] = write end */
    if(pipe(p) == -1) {    
        perror("pipe call error");
        exit(1);
    }
  
    /* Write messages to the pipe */
    strcpy(inbuf, msg1);                   // Copy first message into buffer, includes '\0'
    write(p[1], inbuf, sizeof(inbuf));    // Write entire buffer to pipe (16 bytes)

    strcpy(inbuf, msg2);                   // Copy second message
    write(p[1], inbuf, sizeof(inbuf));    // Write buffer including '\0'

    strcpy(inbuf, msg3);                   // Copy third message
    write(p[1], inbuf, sizeof(inbuf));    // Write buffer including '\0'

    /* Read messages from the pipe */
    for(j = 0; j < 3; j++) {   
        read(p[0], inbuf, sizeof(inbuf));   // Read 16 bytes from pipe
        /* inbuf still contains the null terminator, so printf prints correctly */
        printf("%s\n", inbuf);              
    }
    /*Ecco la prova 3*/
    int x=0;
    exit(1);

/*mettilo nel tuo branch*/

    /*sto aggiungendo frasi nel mio branch*/
    aaaaaaaa
}