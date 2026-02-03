#include "client_handler.h"
#include "session.h"
#include "system_ops.h"
#include "values.h"
#include "permissions.h"
#include "network.h"

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pwd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define CHUNK_SIZE 4096
#include <sys/socket.h>


void login( char *buffer, int client_fd, Session *s){
    
            
             // send login result to client
                char username[64];
                sscanf(buffer + 6, "%63s", username);
                int login_result = session_login(s, username);
            if (login_result == 0) {
                char msg[] = "Login successful\n";
                write(client_fd, msg, strlen(msg));
                memset(buffer, 0, sizeof(&buffer));
                char cwd[PATH_MAX];
                char curr[PATH_MAX];
                dprintf(client_fd,"Cur: %s\n", getcwd(cwd, PATH_MAX));
                snprintf(curr, PATH_MAX, "/%s", s->username);
                chdir(curr);
                dprintf(client_fd,"Cur dopo ch login: %s\n", getcwd(cwd, PATH_MAX));
                
                //send_prompt(client_fd, s);
            } 
            else
             if (login_result == -2) {
                char msg[] = "Already logged in\n";
                write(client_fd, msg, strlen(msg));
            } 
            else {
                char msg[] = "Login failed\n";
                write(client_fd, msg, strlen(msg));
            }

}


void create_user(int client_fd, char *buffer, Session *s){
    // check if logged in 
            if (s->logged_in) {
            char msg[] = "Cannot create user while logged in\n";
            write(client_fd, msg, strlen(msg));
            return;
            }

            char username[64];  
            char perm_str[8];

           if (sscanf(buffer + 12, "%63s %7s", username, perm_str) != 2) {
            char msg[] = COLOR_YELLOW"Use: create_user <username> <permissions>\n"COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
            }
            
            mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t

            if(ensure_user_exists(username)){
                char msg[] = "User already exists\n";
                write(client_fd, msg, strlen(msg));
                return;
            }

            if (sys_create_user(username) == -1){
                char msg[] = "User creation failed\n";
                write(client_fd, msg, strlen(msg));
                return;
            }

            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", s->root_dir, username);


            sys_make_directory(full_path, perms, GROUP_NAME,username);  

            char msg[] = "User created successfully\n";
            write(client_fd, msg, strlen(msg));
            return;
}




//function not necessary not inserted in .h
int check_abs_rel_path(char *raw_path, int client_fd){
     if (raw_path[0] == '/') {
        // È già assoluto (es. /dai/cartella/file)
        if (strlen(raw_path) >= PATH_MAX) {
            char msg[] = "Error: Path too long\n";
            write(client_fd, msg, strlen(msg));
            return -1;
        }
        return 1;
    } else {
        // È relativo
            return -1;
        }
    }











void create(int client_fd, char *buffer, Session *s){
            
            if (!(s->logged_in)) {
                char msg[] = "Cannot create file while you are guest\n";
                write(client_fd, msg, strlen(msg));
                return;
            }
            
            int is_dir = 0; // 0 = file, 1 = directory
            // Pointer that begin after the "create " (quindi a buffer + 7)
            char *args = buffer + 7;
            
            // Search if the next characters are "-d " (note the space after d)
            if (strncmp(args, "-d ", 3) == 0) {
                is_dir = 1;      // We found the option!
                args += 3;       // Shift the pointer of three positions (hop "-d ")
                // Now args point to the beginnig of the path
                }
               
               char path[64];  
               char perm_str[8];

            if (sscanf(args, "%63s %7s", path, perm_str) != 2 ) {
                char msg[] = COLOR_YELLOW"Use: create [-d] <path> <permissions>\n"COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
            }

            mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t
            char full_path[PATH_MAX + 1000];
            /*^ we have increase this parameter to not have warnings^*/
            
            char parent_dir[PATH_MAX];
            char filename_part[64];
            char resolved_parent[PATH_MAX];
        
            // 2. SEPARAZIONE PADRE / FIGLIO
            // Cerchiamo l'ultimo slash per dividere la cartella dal file che vogliamo creare
            char *last_slash = strrchr(path, '/');
    
        if (last_slash != NULL) {
            // Calcoliamo lunghezza della parte directory
            size_t parent_len = last_slash - path;
            
            // Caso speciale: se il file è  direttamente sotto root "/" (es. "/file.txt")
            if (parent_len == 0) {
                strcpy(parent_dir, "/");
            } else {
                strncpy(parent_dir, path, parent_len);
                parent_dir[parent_len] = '\0';
            } 
            
            // Copiamo il nome del file (tutto ciò che c'è dopo lo slash)
            strcpy(filename_part, last_slash + 1);

        } else {
            // Caso teorico (improbabile se gestiamo bene i path sopra):
            // Se non ci sono slash, significa che è nella directory corrente
            char b[PATH_MAX];
            strcpy(parent_dir, getcwd(b,PATH_MAX));
            strcpy(filename_part, path);
        }

         if (strlen(filename_part) == 0) {
            char msg[] = COLOR_RED "Error: Invalid file name (trailing slash?)\n" COLOR_RESET;
            write(client_fd, msg, strlen(msg));
            return;
            }
            if (strcmp(filename_part, ".") == 0 || strcmp(filename_part, "..") == 0) {
            char msg[] = COLOR_RED"Error: Cannot name a file or directory '.' or '..'\n"COLOR_RESET;
            write(client_fd, msg, strlen(msg));
            return;
            }

            dprintf(client_fd,"parent_dir: %s \n",parent_dir);

            if (realpath(parent_dir, resolved_parent) == NULL) {
                dprintf(client_fd, "Error: Destination directory not found or access denied (%s)\n", strerror(errno));
                return;
            }

            //check for home violation creation
            if(check_home_violation(resolved_parent, client_fd, s)==-1) return;

            // Mettiamo insieme la cartella padre pulita + / + il nome file
            snprintf(full_path, PATH_MAX+1000, "%s/%s", resolved_parent, filename_part);

            dprintf(client_fd, "Debug: Creating at %s\n", full_path);

            if(is_dir){
                sys_make_directory_creat(full_path, perms);  
                char msg[] = COLOR_GREEN "Directory created successfully\n" COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
            }
            else{                
                printf("Full path: %s\n", full_path);
                
            int result = sys_make_file(full_path, perms);  
                printf("errno number: %d\n", EEXIST);
                if(result == -EEXIST){
                    char msg[] = COLOR_YELLOW "File already created\n" COLOR_RESET;
                    write(client_fd, msg, strlen(msg));
                    return;
                }else if(result == -EACCES){
                    char msg[] = COLOR_RED "Permission denied\n" COLOR_RESET;
                    write(client_fd, msg, strlen(msg));
                    return;}
                else{
                    char msg[] = COLOR_GREEN "File created successfully\n" COLOR_RESET;
                    write(client_fd, msg, strlen(msg));
                    return;
                }
            }
}





void change_directory(int client_fd, char *buffer, Session *s) {
    
    // 1. Controllo Login
    if (!(s->logged_in)) {
        char msg[] = "Cannot change directory while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    // 2. Parsing del comando (cd <path>)
    char *command = strtok(buffer, " \t\n"); // "cd"
    char *path_arg = strtok(NULL, " \t\n");  // "../foto" o NULL

    if (command == NULL || strncmp(command, "cd", 2) != 0) {
        dprintf(client_fd, "Wrong command!\n");
        return;
    }
    // 3. Gestione del caso "cd" senza argomenti -> vai alla Home
    // NOTA: In ambiente chroot, la home è"/home"
    if (path_arg == NULL) {
        // Se s->home_dir è un path assoluto del sistema reale (es /srv/ftp),
        // Assumiamo che dentro il chroot la home sia la radice "/home" o che s->home_dir sia già adattato.
        char *full = calloc(1, PATH_MAX);
    
        if (full == NULL) {
        perror("Errore allocazione memoria per path");
        exit(1);
        }
        snprintf(full, PATH_MAX, "%s/%s", "/",s->username);
        path_arg = full; 
            
    }
    
    // 4. Risoluzione del percorso con realpath()
    char resolved_path[PATH_MAX];

    char cwd[PATH_MAX];
    dprintf(client_fd,"cwd: %s\n", getcwd(cwd, PATH_MAX));
    
    if (realpath(path_arg, resolved_path) == NULL) {
        // Errore: la cartella non esiste o permessi negati nella risoluzione
        char msg[PATH_MAX + 3000];
        snprintf(msg, sizeof(msg), "Error: Cannot resolve path '%s': %s\n", path_arg, strerror(errno));
        write(client_fd, msg, strlen(msg));
        return;
    }
    dprintf(client_fd, "Path reale: %s\n", resolved_path);
    
    if(check_home_violation(resolved_path, client_fd, s)==-1) return;
    

    if (chdir(resolved_path) == 0) {
        
        // SUCCESS: Aggiorniamo la sessione
        strncpy(s->current_dir, resolved_path, PATH_MAX - 1);
        s->current_dir[PATH_MAX - 1] = '\0';

       
        printf("User %s moved to (chroot): %s\n", s->username, s->current_dir);
        char msg[PATH_MAX + 50];
        snprintf(msg, sizeof(msg), "Directory changed to: %s\n", s->current_dir);
        write(client_fd, msg, strlen(msg));

    } else {
        // FAIL: chdir fallita (es. permessi di esecuzione mancanti sulla cartella finale)
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "Error changing directory to '%s': %s\n", resolved_path, strerror(errno));
        write(client_fd, msg, strlen(msg));
    }
}



void chmods(int client_fd, char *buffer, Session *s){
    if (!(s->logged_in)) {
        char msg[] = "Cannot change mode while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }
        char path[64];  
        char perm_str[8];

        if (sscanf(buffer + 6, "%63s %7s", path, perm_str) != 2 ) {
                char msg[] = COLOR_YELLOW"Use: chmod <path> <permissions>\n"COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
        }

        mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t
        
        char full_path[PATH_MAX + 1000];
        if (realpath(path, full_path) == NULL) {
        // Errore: la cartella non esiste o permessi negati nella risoluzione
        char msg[PATH_MAX + 3000];
        snprintf(msg, sizeof(msg), COLOR_RED"Error: Cannot resolve path '%s': %s\n"COLOR_RESET, path, strerror(errno));
        write(client_fd, msg, strlen(msg));
        return;
        }
        dprintf(client_fd, "Path reale: %s\n", full_path);
    
        if(check_home_violation(full_path, client_fd, s)==-1) return;

//also in chmod the normal user cannot change permission of directories and files that are not his

     /*     struct stat sb;
    
    if (stat(full_path, &sb) == 0 && S_ISDIR(sb.st_mode)  && access(full_path, X_OK) == 0) {
        
        // SUCCESS: directory exist.
        // Update the session with the user
        // The current user have permission to access 
        strncpy(s->current_dir, full_path, PATH_MAX - 1);
        s->current_dir[PATH_MAX - 1] = '\0'; 

        printf("User changed dir to: %s\n", s->current_dir);
        
        char msg[PATH_MAX + 50];
        snprintf(msg, sizeof(msg), "Directory changed to: %s\n", s->current_dir);
        write(client_fd, msg, strlen(msg));

    } else {
        // FAIL: directory does not exist or is not a directory
        char msg[] = "Error: Directory does not exist or you don't have permission\n";
        write(client_fd, msg, strlen(msg));
    } */

        if(chmod(full_path, perms)!=0){
            perror("Error with chmod\n");
            dprintf(client_fd, COLOR_RED"Error with chmod\n"COLOR_RESET);
            return;
        }
        printf("Permission changed correctly\n");
        dprintf(client_fd,COLOR_GREEN"Permission changed correctly\n"COLOR_RESET);
}



void move(int client_fd, char* buffer, Session *s){
        if (!(s->logged_in)) {
            char msg[] = "Cannot move files while you are guest\n";
            write(client_fd, msg, strlen(msg));
            return;
        }

        char path_src[64];
        char path_dest[64];  

        if (sscanf(buffer + 5, "%63s %63s", path_src, path_dest) != 2 ) {
                char msg[] = COLOR_YELLOW"Use: move <path1> <path2>\n"COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
        }

        dprintf(client_fd,"Il path_src: %s\n", path_src);
        dprintf(client_fd,"Il path_dest: %s\n", path_dest);

        char full_path_src[PATH_MAX+1000];
        char full_path_dest[PATH_MAX+1000];
        //now we have to normalize the two path passed and chek also if they are absolute or relative

        if (realpath(path_src, full_path_src) == NULL) {
        // Errore: la cartella non esiste o permessi negati nella risoluzione
            dprintf(client_fd, COLOR_RED"Error: Cannot resolve path '%s': %s\n"COLOR_RESET, path_dest, strerror(errno));
            return;
        }

        if(realpath(path_dest, full_path_dest) == NULL){
            dprintf(client_fd, COLOR_RED"Error: Cannot resolve path '%s': %s\n"COLOR_RESET, path_dest, strerror(errno));
            return;
        }

        dprintf(client_fd, "Path reale_src: %s\n", full_path_src);
        dprintf(client_fd, "Path reale_dest: %s\n", full_path_dest);
    
        if(check_home_violation(full_path_src, client_fd, s)==-1) return;
        if(check_home_violation(full_path_dest, client_fd, s)==-1) return;

        dprintf(client_fd,"Il full_path_src: %s\n", full_path_src);
        dprintf(client_fd,"Il full_path_dest: %s\n", full_path_dest);
        //now we can move the file in the directory of the second path
        move_file(client_fd, full_path_src, full_path_dest);

}




void list(int client_fd, char *buffer, Session *s) {
    if (!(s->logged_in)) {
        char msg[] = "Cannot list files while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    char path[64];
    memset(path, 0, sizeof(path)); 

    // 1. Logica di parsing
    int len1 = strlen(buffer);
    if(len1 == 4) {
        strcpy(path, "."); 
    }else{
    if (sscanf(buffer + 5, "%63s", path) != 1) {
                char msg[] = COLOR_YELLOW"Use: list <path>\n"COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
        }
    }

    //dprintf(client_fd, "Path richiesto: %s\n", path);

    char full_path[PATH_MAX + 1000];
    
    if (realpath(path, full_path)==NULL) {
        dprintf(client_fd, "Error: Path error\n");
        return;
    }


    //dprintf(client_fd, "Path assoluto risolto: %s\n", full_path);

    // 4. Apertura della directory
    DIR *d = opendir(full_path);
    if (!d) {
        char msg[] = "\033[0;31mError: Cannot open directory (check permissions or path)\033[0m\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    // ALLOCAZIONE BUFFER DI RISPOSTA
    // Usiamo malloc per creare un buffer grande dove accumulare tutto il testo.
    // 256KB dovrebbero bastare per liste anche molto lunghe.
    size_t resp_size = 262144; 
    char *response_buffer = malloc(resp_size);
    if (!response_buffer) {
        perror("malloc list buffer");
        closedir(d);
        return;
    }
    memset(response_buffer, 0, resp_size);

    
    struct dirent *dir;
    struct stat file_stat;
    char line_buffer[2048];                
    char filepath_for_stat[PATH_MAX + 2000]; 
    char perm_str[12];                       

    // Scriviamo l'intestazione nel buffer accumulatore
    snprintf(line_buffer, sizeof(line_buffer), "%-12s %-30s %-15s\n", "PERMISSIONS", "NAME", "TOTAL SIZE");
    strcat(response_buffer, line_buffer);
    strcat(response_buffer, "------------------------------------------------------------\n");

    while ((dir = readdir(d)) != NULL) {
        snprintf(filepath_for_stat, sizeof(filepath_for_stat), "%s/%s", full_path, dir->d_name);

        if (stat(filepath_for_stat, &file_stat) == 0) {
            
            get_perm_string(file_stat.st_mode, perm_str);

            long long display_size = file_stat.st_size;//to upgrade, it has to show the logical size of directory not total

            // Se è una directory, calcoliamo la dimensione ricorsiva
            if (S_ISDIR(file_stat.st_mode)) {
                if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                    display_size += get_directory_content_size(filepath_for_stat);
                }
            }

            // Formattiamo la riga
            snprintf(line_buffer, sizeof(line_buffer), "%-12s %-30s %lld bytes\n", 
                     perm_str, 
                     dir->d_name, 
                     display_size);
            
            // Controlliamo di non sforare il buffer totale
            if (strlen(response_buffer) + strlen(line_buffer) < resp_size - 1) {
                strcat(response_buffer, line_buffer);
            } else {
                strcat(response_buffer, "... output truncated (too long) ...\n");
                break;
            }
        }
    }

    // 6. INVIO UNICO AL CLIENT
    // Ora inviamo tutto il blocco insieme. Il client riceverà tutto in una volta (o quasi),
    // senza le pause causate dai calcoli ricorsivi.
    write(client_fd, response_buffer, strlen(response_buffer));

    // Pulizia
    free(response_buffer);
    closedir(d);
}




void delete(int client_fd, char* buffer, Session *s){
    if (!(s->logged_in)) {
        char msg[] = "Cannot list files while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    char path[64];
    memset(path, 0, sizeof(path)); 

    // 1. Logica di parsing
    if (sscanf(buffer + 7, "%63s", path) != 1) {
        dprintf(client_fd,"Usage: delete <path>\n");
        return;
    }
    
    dprintf(client_fd, "Path richiesto: %s\n", path);

    char full_path[PATH_MAX + 1000];


    if (realpath(path, full_path)==NULL) {
        dprintf(client_fd, "Error: Path error\n");
        return;
    }
    if(check_home_violation(full_path,client_fd, s)==-1)return;

    // Restituisce 0 in caso di successo, -1 in caso di errore
    if (unlink(full_path) == 0) {
        printf("File eliminato con successo.\n");
        dprintf(client_fd, COLOR_GREEN"File deleted correctly: %s\n"COLOR_RESET, get_last(path, client_fd));

    } else {
        perror("Errore durante l'eliminazione");
        dprintf(client_fd, COLOR_RED"Error with elimination of: %s\n"COLOR_RESET, get_last(path, client_fd));
    }
}







void write_client(int client_fd, char* buffer, Session *s){
    if (!(s->logged_in)) {
        char msg[] = "Cannot list files while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }


    int is_set = 0; 
    char *args = buffer + 6;
    int num;
    int consumed=0;
    
    if (strncmp(args, "-offset ", 8) == 0) {
        printf("Sto per aggiungere off..\n");
        is_set = 1;      // We found the option!
        args += 8;       // Shift the pointer of three positions (hop " ")
        int i;
        if( (i=sscanf(args, "%d%n", &num, &consumed))==1){//%*[^0-9]%d  ---> hop all the non integer input , %n per contare
        args += consumed;
        // Now args point to the beginnig of the path
    }else{
        write(client_fd,"NO_OFF",6);
        printf("Fail to save num\n");
        return;
    }
        printf("args e': %s, cosumed:%d,scanf readed %d\n", args, consumed, i);
    }else if(strncmp(args, "-offset", 7)==0){
        write(client_fd,"US",2);
        printf("Fail to save off\n");
        return;
    }else{
        
    }

    char path[64];
    memset(path, 0, sizeof(path)); 

    // 1. Logica di parsing, and we add to args the number length and a space..
    if (sscanf(args, "%63s", path) != 1) {
        write(client_fd,"ERROR",5);
        //dprintf(client_fd,"Usage: from server write -offset=<num> <path>\n");
        return;
    }

        char full_path[PATH_MAX + 1000];
        char parent_dir[PATH_MAX];
        char filename_part[64];
        char resolved_parent[PATH_MAX];
    
        // 2. SEPARAZIONE PADRE / FIGLIO
        // Cerchiamo l'ultimo slash per dividere la cartella dal file che vogliamo creare
        char *last_slash = strrchr(path, '/');
    
        if (last_slash != NULL) {
            // Calcoliamo lunghezza della parte directory
            size_t parent_len = last_slash - path;
            
            // Caso speciale: se il file è  direttamente sotto root "/" (es. "/file.txt")
            if (parent_len == 0) {
                strcpy(parent_dir, "/");
            } else {
                strncpy(parent_dir, path, parent_len);
                parent_dir[parent_len] = '\0';
            } 
            
            // Copiamo il nome del file (tutto ciò che c'è dopo lo slash)
            strcpy(filename_part, last_slash + 1);

        } else {
            // Caso teorico (improbabile se gestiamo bene i path sopra):
            // Se non ci sono slash, significa che è nella directory corrente
            char b[PATH_MAX];
            strcpy(parent_dir, getcwd(b,PATH_MAX));
            strcpy(filename_part, path);
        }

         if (strlen(filename_part) == 0) {
             char msg[] = COLOR_RED "Error: Invalid file name (trailing slash?)\n" COLOR_RESET;
             write(client_fd, msg, strlen(msg));
             return;
            }
            if (strcmp(filename_part, ".") == 0 || strcmp(filename_part, "..") == 0) {
                char msg[] = COLOR_RED"Error: Cannot name a file or directory '.' or '..'\n"COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
            }
            
            
            if (realpath(parent_dir, resolved_parent) == NULL) {
                dprintf(client_fd, COLOR_RED"Error: Destination directory not found or access denied (%s)\n"COLOR_RESET, strerror(errno));
                return;
            }
            
            //check for home violation creation
            if(check_home_violation(resolved_parent, client_fd, s)==-1) return;
            int file_fd;
            // Mettiamo insieme la cartella padre pulita + / + il nome file
            snprintf(full_path, PATH_MAX+1000, "%s/%s", resolved_parent, filename_part);

            if(is_set){
                file_fd = open(full_path, O_CREAT | O_RDWR, S_IRWXU);
            }else{
                file_fd = open(full_path, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU); //ci andrebbe append al posto di trunc
            }

            if (file_fd < 0) {
                dprintf(client_fd, "Error creating file '%s': %s\n", full_path, strerror(errno));
                return;
            }

            
    //dprintf(client_fd, "Ready. Type content. Type 'END' on a new line to finish.\n");
            
    char line_buf[2048];
            
            
    //With the offset, we have to insert the new strings without cancel the rest of file
    struct stat st;
    fstat(file_fd, &st);

    off_t size = st.st_size;

    //check if the offset length is longer than the file size.
    if(size < num){
        write(client_fd, "OFF_ER",6);
        //dprintf(client_fd,COLOR_RED"Error: offset length too long!\n"COLOR_RESET); 
        return;
    }else{
        write(client_fd, "OK", 2);
    }

    off_t tail_size = size - num;
    printf("File_size:%ld, Tail_size: %ld\n", size, tail_size);
    char *tail = malloc(tail_size);
    if(is_set){
            lseek(file_fd, num, SEEK_SET);
            while(read(file_fd, tail, tail_size)<0) break;
            printf("tail: %s\n", tail);
            lseek(file_fd, num, SEEK_SET);
    }

    // 4. Cycle to read 
    // On the other side we use fgets in client
    while (1) {
        
        // Legge una riga dal socket (gestisce blocchi e segnali)
        ssize_t n = read(client_fd, line_buf, sizeof(line_buf));
        printf("Letto n: %ld\n", n);
        if (n <= 0) break;

        // 5. Controllo "END"
        // Controlliamo tutte le varianti di a capo
        // char *terminator = strstr(line_buf, "END");
        // if (terminator != NULL) {
        //     break;
        //     // printf("E' entrato qui? Allora ha preso end=%s\n", terminator);
        //     // // Trovato! Calcoliamo quanti byte scrivere prima di :end
        //     // int bytes_to_write = terminator - line_buf;
        //     // printf("Quanti sono dalla sottrazione terminatore-linebuff?=%d\n", bytes_to_write);
        //     // if (bytes_to_write > 0) {
        //     //     printf("Scrivo qui?\n");
        //     //     write(file_fd, line_buf, bytes_to_write);
        //     //     break;
        //     // }
        // }
        if (strcmp(line_buf, "END\n") == 0 || strcmp(line_buf, "END") == 0) {
            printf("Transfer completed.\n");
            dprintf(client_fd, COLOR_GREEN"File received and saved successfully.\n"COLOR_RESET);
            break; // Esce dal ciclo while
        }

        //per inserire e non sovrascrivere quello che già c'è:
        printf("Qua ci arriva a leggere\n");
        if (write(file_fd, line_buf, n) < 0) {
            dprintf(client_fd, "Error writing to file: %s\n", strerror(errno));
            break;
            }
    }

    if(is_set){
    write(file_fd, tail, tail_size);
    }
    free(tail);

    // Chiudiamo il file che abbiamo scritto
    close(file_fd);


}


void upload (int client_fd, char* command_args, Session *s){

    if (!(s->logged_in)) {
        char msg[] = "Cannot upload files while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }
    char filepath[256];
    long filesize;
    
    // 1. Parsing degli argomenti
    if (sscanf(command_args, "upload %255s %ld", filepath, &filesize) != 2) {
        char *msg = "ERROR: Invalid format. Use: upload <filename> <size>\n";
        send_message(client_fd, msg);
        //send(client_fd, msg, strlen(msg), 0);
        return;
    }

    char final_safe_path[PATH_MAX];

    if (resolve_safe_create_path(filepath, client_fd, s, final_safe_path) == -1) {
        return;
    }

    printf("Incoming file: %s (%ld bytes)\n", filepath, filesize);

    // 2. Apre il file in scrittura BINARIA ("wb")
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        perror("File creation failed");
        char *msg = "ERROR: Cannot create file on server.\n";
        send(client_fd, msg, strlen(msg), 0);
        return;
    }

    char *ack = "READY";
    send_message(client_fd, ack);
    //send(client_fd, ack, strlen(ack), 0);

    char buffer[BUFFER_SIZE];
    long total_received = 0;
    int bytes_received;

    // 3. Riceve i dati in un loop fino a completare il file
    while (total_received < filesize) {

        long bytes_left = filesize - total_received;
        int bytes_to_read = (bytes_left < (long)sizeof(buffer)) ? bytes_left : (long)sizeof(buffer);

        bytes_received = recv(client_fd, buffer, bytes_to_read, 0);
        if (bytes_received <= 0) {
            perror("Receive error");
            break;
        }

        fwrite(buffer, 1, bytes_received, fp);
        total_received += bytes_received;
        printf("\r[Server] Ricevuti: %ld / %ld \n", total_received, filesize);
    }
    fflush(fp);
    fclose(fp);


    char *msg = "SUCCESS";
    send(client_fd, msg, strlen(msg), 0);
    printf("Upload completato. Inviato ACK.\n");
    
    printf("File received successfully: %s\n", filepath);
}









    void read_client(int client_fd, char *buffer, Session *s){
    if (!(s->logged_in)) {
        char msg[] = "Cannot list files while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    int is_set = 0; 
    char *args = buffer + 5;
    int num=0;
    int consumed=0;
    
    if (strncmp(args, "-offset ", 8) == 0) {
        printf("adding offset..\n");
        is_set = 1;      // We found the option!
        args += 8;       // Shift the pointer of three positions (hop " ")
        int i;
        if( (i=sscanf(args, "%d%n", &num, &consumed))==1){//%*[^0-9]%d  ---> hop all the non integer input , %n per contare
        args += consumed;
        // Now args point to the beginnig of the path
    }else{
        write(client_fd,"NO_OFF",6);
        printf("Fail to save num\n");
        return;
    }
        printf("args is: %s, cosumed instead:%d, readed %d\n", args, consumed, i);
    }else if(strncmp(args, "-offset", 7)==0) {
        write(client_fd,"US",2);
        printf("Fail to save off\n");
        return;
    }else{
        write(client_fd,"OK",2);
    }

    char path[64];
    memset(path, 0, sizeof(path)); 

    // 1. Parsing logic, and we add to args the number length and a space..
    if (sscanf(args, "%63s", path) != 1) {
        printf("Usage: read -offset=<num> <path>\n");
        return;
    }

    char full_path[PATH_MAX + 1000];

    if (realpath(path, full_path) == NULL) {
        dprintf(client_fd, COLOR_RED"Error: Destination directory not found or access denied (%s)\n"COLOR_RESET, strerror(errno));
        return;
    }
            
    //check for home violation creation
    if(check_home_violation(full_path, client_fd, s)==-1) return;

    int file_fd;
    file_fd = open(full_path, O_RDONLY);
    

    if (file_fd < 0) {
        dprintf(client_fd, "Error creating file '%s': %s\n", full_path, strerror(errno));
        return;
    }
                   
    char line_buf[2048];
    char line_control[2048];
    //With the offset, we have to insert the new strings without cancel the rest of file
    struct stat st;
    fstat(file_fd, &st);
    off_t size = st.st_size;
    snprintf(line_control, sizeof(line_control), "The size of file is :%ld, instead the offset is:%d error offset too long!", size, num);
    //write(client_fd, line_control, sizeof(line_control));
    //check if the offset length is longer than the file size.
    if(size < num){
        write(client_fd, "ERR_OFFSET", 10);
        printf("Ho inviato errore offs, esco dalla funzione read_client e torno nel while principale\n");
        //write(client_fd, line_control, sizeof(line_control));
        //dprintf(client_fd,COLOR_RED"Error: offset length too long!\n"COLOR_RESET);
        close(file_fd);  
        return;
    }

    if(is_set){
    lseek(file_fd, num, SEEK_SET);
    }

    ssize_t l = read(file_fd, line_buf, sizeof(line_buf));
    printf("Letto l da server: %ld\n", l);
    if(l<=0){
        write(client_fd, "EMPTY_OR_READ_ERROR", 19);
        return;
    }

    if (write(client_fd, line_buf, l) < 0) {
        dprintf(client_fd, "Error writing to file: %s\n", strerror(errno));
        return;
        }
        //send_prompt(client_fd,s);
    printf("At the end of write from server\n");
    lseek(file_fd, 0, SEEK_SET);
    close(file_fd);
    
    }
    
    
void download(int client_fd, char* command_args, Session *s){

    if (!(s->logged_in)) {
        char msg[] = "Cannot download files while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    char filepath[256];

    // Parsing of arguments
    if (sscanf(command_args, "download %255s", filepath) != 1) {
        char *msg = "ERROR: Invalid format. Use: download <server path> <client path>\n";
        send_message(client_fd, msg);
        return;
    }

    char final_safe_path[PATH_MAX];
    if (resolve_safe_create_path(filepath, client_fd, s, final_safe_path) == -1) {
        return;
    }

    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("File open failed");
        char *msg = "ERROR: Cannot open file on server.\n";
        send_message(client_fd, msg);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char size_msg[64];
    snprintf(size_msg, sizeof(size_msg), "SIZE %ld", filesize);
    send_message(client_fd, size_msg);

    char ack_buffer[16] = {0};
    receive_message(client_fd, ack_buffer, sizeof(ack_buffer));

    if (strncmp(ack_buffer, "READY", 5) != 0) {
        printf("Client denied download: %s\n", ack_buffer);
        fclose(fp);
        return;
    }

    

    char data_buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t bytes_sent = 0;

    
    while (bytes_sent < (size_t)filesize) {
        size_t to_send = fread(data_buffer, 1, sizeof(data_buffer), fp);
            if (to_send > 0) {
                send_message(client_fd, data_buffer);
                bytes_sent += to_send;
                printf("\r[Client] Inviati: %ld byte...", bytes_sent);
            }
    }
    
    fclose(fp);
    printf("File inviato. Attendo conferma dal server...\n");


    char ack[16]= {0};
    if (recv(client_fd, ack, sizeof(ack), 0) <= 0) {
        perror("Errore ricezione conferma");
    } else {
        if (strncmp(ack, "SUCCESS", 7) == 0) {
            printf(COLOR_GREEN "File downloaded successfully." COLOR_RESET "\n");
        } else {
            printf("Errore dal server: %s\n", ack);
        }
    }
    
}