#include "client_handler.h"
#include "session.h"
#include "system_ops.h"
#include "values.h"
#include "permissions.h"
#include "network.h"
#include "shared.h"

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
#include <sys/file.h>
#include <signal.h>

#define SIZE 6000
#include <sys/socket.h>


void login( char *buffer, int client_fd, Session *s, SharedMemory *shm){
    
            
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
                
                register_user(shm, username);
                
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
    
    // Controllo Login
    if (!(s->logged_in)) {
        char msg[] = "Cannot change directory while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    //Parsing del comando (cd <path>)
    char *command = strtok(buffer, " \t\n"); 
    char *path_arg = strtok(NULL, " \t\n"); 

    if (command == NULL || strncmp(command, "cd", 2) != 0) {
        dprintf(client_fd, "Wrong command!\n");
        return;
    }
    // Gestione del caso "cd" senza argomenti -> vai alla Home
    // In ambiente chroot, la home è"/home"
    if (path_arg == NULL) {
        // Se s->home_dir è un path assoluto del sistema reale,
        // Assumiamo che dentro il chroot la home sia la radice "/home" o che s->home_dir sia già adattato.
        char *full = calloc(1, PATH_MAX);
    
        if (full == NULL) {
        perror("Errore allocazione memoria per path");
        exit(1);
        }
        snprintf(full, PATH_MAX, "%s/%s", "/",s->username);
        path_arg = full; 
            
    }
    
    //Risoluzione del percorso con realpath()
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
    dprintf(client_fd, "Real path: %s\n", resolved_path);
    
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
        dprintf(client_fd, "Real path: %s\n", full_path);
    
        if(check_home_violation(full_path, client_fd, s)==-1) return;



    //implementare il lock per cartelle 
    int fd = open(full_path, O_RDONLY);
    if(errno == EACCES){
        printf("You can procede, owner have permission\n");
    }else if (fd == -1) {
        dprintf(client_fd, COLOR_RED"Error opening file '%s': %s\n"COLOR_RESET, full_path, strerror(errno));
        return;
        }

        if(lock_commands(fd,client_fd,0,0)!=0){
        return;
        }
         

        
        if(chmod(full_path, perms)!=0){
            perror("Error with chmod\n");
            dprintf(client_fd, COLOR_RED"Error with chmod\n"COLOR_RESET);
            unlock(fd);
            close(fd);
            return;
        }

        unlock(fd);
        close(fd);
        printf("Permission changed correctly\n");
        dprintf(client_fd,COLOR_GREEN"Permission changed correctly\n"COLOR_RESET);
}



void move(int client_fd, char* buffer, Session *s){
        if (!(s->logged_in)) {
            char msg[] = "Cannot move files while you are guest\n";
            write(client_fd, msg, strlen(msg));
            return;
        }

        char *path_src;
        char *path_dest;

        // buffer + 5 salta "move "
        char *args = buffer + 5;

        // Prendi il primo argomento
        path_src = strtok(args, " \n\r\t");
        // Prendi il secondo argomento
        path_dest = strtok(NULL, " \n\r\t");

        if (path_src == NULL || path_dest == NULL) {
            char msg[] = COLOR_YELLOW"Use: move <path_src> <path_dest>\n"COLOR_RESET;
            write(client_fd, msg, strlen(msg));
            return;
        }
        
        dprintf(client_fd,"Il path_src: %s\n", path_src);
        dprintf(client_fd,"Il path_dest: %s\n", path_dest);

        char full_path_src[PATH_MAX+1000];


        if (realpath(path_src, full_path_src) == NULL) {
        // Errore: la cartella non esiste o permessi negati nella risoluzione
            dprintf(client_fd, COLOR_RED"Error: Cannot resolve path '%s': %s\n"COLOR_RESET, path_dest, strerror(errno));
            return;
        }

       

        dprintf(client_fd, "Path reale_src: %s\n", full_path_src);
        
    
        if(check_home_violation(full_path_src, client_fd, s)==-1) return;
        
        
        
    char full_path_dest_res[PATH_MAX+1000];
    struct stat st_dest;
    int dest_exists = (stat(path_dest, &st_dest) == 0);
    
    if (dest_exists && S_ISDIR(st_dest.st_mode)) {
        // CASO A: Il path_dest è una directory. 
        // Dobbiamo appendere il nome del file originale.
        char *filename = custom_basename(full_path_src);
        char temp_path[PATH_MAX];
        
        snprintf(temp_path, sizeof(temp_path), "%s/%s", path_dest, filename);
        
        // Ora risolviamo questo nuovo path combinato
        if (resolve_safe_create_path(temp_path, client_fd, s, full_path_dest_res) != 0) {
            return; // Errore già gestito dentro resolve_safe_create_path
        }
    } 
    else {
        //CASO B: Il path_dest non esiste o include già un (nuovo) filename
        // Usiamo direttamente la tua logica di risoluzione sicura
        if (resolve_safe_create_path(path_dest, client_fd, s, full_path_dest_res) != 0) {
            return;
        }
    }

        dprintf(client_fd,"Il full_path_src: %s\n", full_path_src);
        dprintf(client_fd,"Il full_path_dest: %s\n", full_path_dest_res);
        
        int fd = open(full_path_src, O_WRONLY);
        if (fd == -1) {
        dprintf(client_fd, COLOR_RED"Error opening or creating file '%s': %s\n"COLOR_RESET, full_path_src, strerror(errno));
        return;
        }

        if(lock_commands(fd,client_fd,1,0)!=0){
        return;
        }
        
        //now we can move the file in the directory of the second path
        move_file(client_fd, full_path_src, full_path_dest_res);
        unlock(fd);
        close(fd);
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
    if(len1 == 4 || len1==5) {
        strcpy(path, "."); 
    }else{
    if (sscanf(buffer + 5, "%63s", path) != 1) {
                char msg[] = COLOR_YELLOW"Use: list <path>\n"COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
        }
    }

    

    char full_path[PATH_MAX + 1000];
    
    if (realpath(path, full_path)==NULL) {
        dprintf(client_fd, "Error: Path error\n");
        return;
    }


    

    // 4. Apertura della directory
    DIR *d = opendir(full_path);
    if (!d) {
        char msg[] = "\033[0;31mError: Cannot open directory (check permissions or path)\033[0m\n";
        write(client_fd, msg, strlen(msg));
        return;
    }else if(errno == EACCES){
        printf("Is not possible to open dir\n");
        dprintf(client_fd,"Is not possible to open dir");
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

    write(client_fd, response_buffer, strlen(response_buffer));

    // Pulizia
    free(response_buffer);
    closedir(d);
}




void delete(int client_fd, char* buffer, Session *s){
   if(!is_logged_in(client_fd,s)){
        return;
    }

    char path[64];
    memset(path, 0, sizeof(path)); 

    // 1. Logica di parsing
    if (sscanf(buffer + 7, "%63s", path) != 1) {
        dprintf(client_fd,"Usage: delete <path>\n");
        return;
    }
    
    dprintf(client_fd, "Requested Path: %s\n", path);

    char full_path[PATH_MAX + 1000];


    if (realpath(path, full_path)==NULL) {
        dprintf(client_fd, "Error: Path error\n");
        return;
    }
    if(check_home_violation(full_path,client_fd, s)==-1)return;


    int fd = open(full_path, O_WRONLY);
    if (fd == -1) {
        dprintf(client_fd, COLOR_RED"Error opening file '%s': %s\n"COLOR_RESET, full_path, strerror(errno));
        return;
    }


    if(lock_commands(fd,client_fd,1,0)!=0){
        return;
    }
    
    // Restituisce 0 in caso di successo, -1 in caso di errore
    if (unlink(full_path) == 0) {
        printf("File eliminato con successo.\n");
        dprintf(client_fd, COLOR_GREEN"File deleted correctly: %s\n"COLOR_RESET, get_last(path, client_fd));
        unlock(fd);
        close(fd);
    } else {
        perror("Errore durante l'eliminazione");
        dprintf(client_fd, COLOR_RED"Error with elimination of: %s\n"COLOR_RESET, get_last(path, client_fd));
        unlock(fd);
        close(fd);
    }
}



void write_client(int client_fd, char* buffer, Session *s){
    if (!(s->logged_in)) {
        uint32_t codice = htonl(RESP_ERR_GUEST); // Converte l'1 in formato rete
        write(client_fd, &codice, sizeof(uint32_t));
        return;
    }


    int is_set = 0; 
    char *args = buffer + 6;
    long num;
    int consumed=0;
    
    if (strncmp(args, "-offset ", 8) == 0) {
        printf("Sto per aggiungere off..\n");
        is_set = 1;      // We found the option!
        args += 8;       // Shift the pointer of three positions (hop " ")
        int i;
        if( (i=sscanf(args, "%ld%n", &num, &consumed))==1){//%*[^0-9]%d  ---> hop all the non integer input , %n per contare
        args += consumed;
        if(num<0){
  
        uint32_t codice = htonl(RESP_ERR_OFFSET_UNDER); 
        write(client_fd, &codice, sizeof(uint32_t));
        return;
        }
        // Now args point to the beginnig of the path
    }else{

        uint32_t codice = htonl(RESP_ERR_NO_OFFSET); 
        write(client_fd, &codice, sizeof(uint32_t));
        return;
    }
        printf("args e': %s, cosumed:%d,scanf readed %d\n", args, consumed, i);
    }else if(strncmp(args, "-offset", 7)==0){
 
        uint32_t codice = htonl(RESP_USAGE); 
        write(client_fd, &codice, sizeof(uint32_t));
        return;
    }

    char path[64];
    memset(path, 0, sizeof(path)); 

    //Logica di parsing, and we add to args the number length and a space..
    if (sscanf(args, "%63s", path) != 1) {

        uint32_t codice = htonl(RESP_USAGE); 
        write(client_fd, &codice, sizeof(uint32_t));
        return;
    }
        printf("Il path passato adesso e':%s\n",path);
        char full_path[PATH_MAX + 1000];
        char parent_dir[PATH_MAX];
        char filename_part[64];
        char resolved_parent[PATH_MAX];
    
        // SEPARAZIONE PADRE / FIGLIO
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
 
            uint32_t codice = htonl(RESP_ERR_INVALID_FILE_N);
            write(client_fd, &codice, sizeof(uint32_t));
            return;
            }
            if (strcmp(filename_part, ".") == 0 || strcmp(filename_part, "..") == 0) {

                uint32_t codice = htonl(RESP_ERR_INVALID_DOT);
                write(client_fd, &codice, sizeof(uint32_t));
                return;
            }
            if (strstr(filename_part, "-")) {
 
                uint32_t codice = htonl(RESP_ERR_INVALID_NAME);
                write(client_fd, &codice, sizeof(uint32_t));

                return;
            }
            
            
            if (realpath(parent_dir, resolved_parent) == NULL) {

                uint32_t codice = htonl(RESP_ERR_PATH);
                write(client_fd, &codice, sizeof(uint32_t));

                return;
            }
            
            //check for home violation creation
            if(check_home_violation_r(resolved_parent, client_fd, s)==-1) {

                uint32_t codice = htonl(RESP_ERR_VIO);
                write(client_fd, &codice, sizeof(uint32_t));
                return;
            }

            // Mettiamo insieme la cartella padre pulita + / + il nome file
            snprintf(full_path, PATH_MAX+1000, "%s/%s", resolved_parent, filename_part);
            
            int file_fd;
            if(is_set){
                file_fd = open(full_path, O_CREAT | O_RDWR, S_IRWXU);
            }else{
                file_fd = open(full_path, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU); 
            }
            
            if (file_fd < 0) {

                uint32_t codice = htonl(RESP_ERR_OPEN);
                write(client_fd, &codice, sizeof(uint32_t));
                return;
            }
            if(lock_commands(file_fd,client_fd,1,1)!=0){
                printf("Occupied, so return\n");
                return;
            }
            
            
    char line_buf[2048]; 
            
    //With the offset, we have to insert the new strings without cancel the rest of file
    struct stat st;
    fstat(file_fd, &st);

    off_t size = st.st_size;

    //check if the offset length is longer than the file size.
    if(is_set && size < num){

        uint32_t codice = htonl(RESP_ERR_OFFSET_LONG);
        write(client_fd, &codice, sizeof(uint32_t));
        unlock(file_fd);
        close(file_fd); 
        return;
     }else{
 
        uint32_t codice = htonl(RESP_OK);
        write(client_fd, &codice, sizeof(uint32_t));
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
    unlock(file_fd);
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
        return;
    }

    char final_path[PATH_MAX];

    if (resolve_safe_create_path(filepath, client_fd, s, final_path) == -1) {
        return;
    }

    printf("Incoming file: %s (%ld bytes)\n", filepath, filesize);

    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        perror("File creation failed");
        char *msg = "ERROR: Cannot create file on server.\n";
        send(client_fd, msg, strlen(msg), 0);
        return;
    }

    int fd = fileno(fp); 

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        printf(COLOR_RED "[SERVER] File unavailable.\n" COLOR_RESET);

        char err_msg[] = "ERROR: File is locked by another user.\n";
        send(client_fd, err_msg, strlen(err_msg), 0);
        fclose(fp);
        return;
    }

    char *ack = "READY";
    send_message(client_fd, ack);

    char buffer[BUFFER_SIZE];
    long total_received = 0;
    int bytes_received;

    // 3. Receive loop
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
        printf("\r[Server] Received: %ld / %ld \n", total_received, filesize);
    }
    
    fflush(fp);
    fclose(fp);


    char *msg = "SUCCESS";
    send(client_fd, msg, strlen(msg), 0);
    printf("Upload completed successfully.\n");
    
    printf("File received successfully: %s\n", filepath);
}


    

void read_client(int client_fd, char *buffer, Session *s) {
    if (!(s->logged_in)) {
        char err_msg[] = "Cannot read files while you are guest";
        uint32_t err_size = htonl(strlen(err_msg));
        write(client_fd, &err_size, sizeof(uint32_t));
        write(client_fd, err_msg, strlen(err_msg));
        return;
    }

    int is_set = 0; 
    char *args = buffer + 5;
    int num = 0;
    int consumed = 0;
    
    // Gestione Offset
    if (strncmp(args, "-offset ", 8) == 0) {
        is_set = 1;
        args += 8;
        if (sscanf(args, "%d%n", &num, &consumed) == 1) {
            args += consumed;
        } else {
            uint32_t err_size = htonl(2);
            write(client_fd, &err_size, sizeof(uint32_t));
            write(client_fd, "US", 2);
            return;
        }
    } else if (strncmp(args, "-offset", 7) == 0) {
        uint32_t err_size = htonl(2);
        write(client_fd, &err_size, sizeof(uint32_t));
        write(client_fd, "US", 2);
        return;
    }

    char path[64];
    memset(path, 0, sizeof(path)); 
    if (sscanf(args, "%63s", path) != 1) {
        // Errore silenzioso o log lato server
        char *err_msg = "ERROR_PARAM";
        uint32_t err_size = htonl(strlen(err_msg));
        write(client_fd, &err_size, sizeof(uint32_t));
        write(client_fd, err_msg, strlen(err_msg));
        return;
    }

    char full_path[PATH_MAX];
    if (realpath(path, full_path) == NULL) {
        // Se il file non esiste, avvisa il client
        char *err_msg = "ERR_NOT_FOUND";
        uint32_t err_size = htonl(strlen(err_msg));
        write(client_fd, &err_size, sizeof(uint32_t));
        write(client_fd, err_msg, strlen(err_msg));
        return;
    }
            
    if (check_home_violation_r(full_path, client_fd, s) == -1){
        char *err_msg = "ERR_VIO";
        uint32_t err_size = htonl(strlen(err_msg));
        write(client_fd, &err_size, sizeof(uint32_t));
        write(client_fd, err_msg, strlen(err_msg));
        return;
    }
    

    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        char *err_msg = "ERR_OPEN";
        uint32_t err_size = htonl(strlen(err_msg));
        write(client_fd, &err_size, sizeof(uint32_t));
        write(client_fd, err_msg, strlen(err_msg));
        return;
    }
         
    if (lock_commands(file_fd, client_fd, 0, 2) != 0) {
        char *err_msg = "ERROR_LOCKED";
        uint32_t err_size = htonl(strlen(err_msg));
        write(client_fd, &err_size, sizeof(uint32_t));
        write(client_fd, err_msg, strlen(err_msg));
        return;
    }

    struct stat st;
    fstat(file_fd, &st);
    off_t file_size = st.st_size;

    // Controllo Offset troppo grande
    if (is_set && file_size < num) {
        char *err_msg = "ERR_OFFSET";
        uint32_t err_size = htonl(strlen(err_msg));
        write(client_fd, &err_size, sizeof(uint32_t));
        write(client_fd, err_msg, strlen(err_msg));
        unlock(file_fd);
        close(file_fd);  
        return;
    }

    // Calcolo quanti byte effettivamente invieremo
    uint32_t effective_size = (is_set) ? (file_size - num) : file_size;
    uint32_t net_count = htonl(effective_size);
    
    
    write(client_fd, &net_count, sizeof(uint32_t));

    if (is_set) {
        lseek(file_fd, num, SEEK_SET);
    }

    // Invio del contenuto a pezzi
    char line_buf[SIZE]; 
    ssize_t l;
    while ((l = read(file_fd, line_buf, sizeof(line_buf))) > 0) {
        if (write(client_fd, line_buf, l) < 0) {
            break;
        }
    }

    printf("Inviati %u byte al client.\n", effective_size);
    unlock(file_fd);
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

    char final_path[PATH_MAX];

    if (resolve_safe_create_path(filepath, client_fd, s, final_path) == -1) {
        return;
    }

    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("File open failed");
        char *msg = "ERROR: Cannot open file on server.\n";
        send_message(client_fd, msg);
        return;
    }

   

    int fd = fileno(fp);
    if(flock(fd, LOCK_SH | LOCK_NB) != 0) {
        printf("Cannot open file, already locked.\n");
        char *msg = COLOR_RED"ERROR: Cannot lock file on server.\n"COLOR_RESET;
        send_message(client_fd, msg);
        fclose(fp);
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
    
    size_t bytes_sent = 0;

    
    while (bytes_sent < (size_t)filesize) {
        size_t to_send = fread(data_buffer, 1, sizeof(data_buffer), fp);
            if (to_send > 0) {
                send_message(client_fd, data_buffer);
                bytes_sent += to_send;
                printf("\r[Client] Send: %ld byte...", bytes_sent);
            }
    }
    
    fclose(fp);
    printf("Waiting for server confirmation...\n");


    char ack[16]= {0};
    if (recv(client_fd, ack, sizeof(ack), 0) <= 0) {
        perror("Error receiving final ACK");
    } else {
        if (strncmp(ack, "SUCCESS", 7) == 0) {
            printf(COLOR_GREEN "File downloaded successfully." COLOR_RESET "\n");
        } else {
            printf("Server error: %s\n", ack);
        }
    }

    
    
}


void transfer_request(int client_fd, char* buffer, SharedMemory *shm, Session *s){

        if (!(s->logged_in)) {
            char msg[] = "Cannot transfer files while you are guest\n";
            write(client_fd, msg, strlen(msg));
            return;
        }

        pid_t target_pid = -1;
        int req_idx = -1;

        char filename[64];
        char target_user[64];

        if (sscanf(buffer, "transfer_request %255s %255s", filename, target_user) != 2) {
            char *msg = "ERROR: Invalid format. Use: transfer_request <source file> <destination user>\n";
            send_message(client_fd, msg);
            return;
        }

        char full_path[PATH_MAX];

        // if (realpath(filename, full_path) == NULL) {
        //     dprintf(client_fd, COLOR_RED"Error: Cannot resolve file path '%s': %s\n"COLOR_RESET, filename, strerror(errno));
        //     send_message(client_fd, COLOR_RED"ERROR: Cannot resolve file path.\n"COLOR_RESET);
        //     return;
        // }
        

        
        printf("Transfer request: file '%s' to user '%s'\n", full_path, target_user);
        while(target_pid == -1){
            sem_wait(&shm->semaphore);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (shm->users[i].is_online && strcmp(shm->users[i].username, target_user) == 0) {

                    target_pid = shm->users[i].pid;
                    
                    break;
                }

            }
            sem_post(&shm->semaphore);
            sleep(1);
        }

        if(sem_wait(&shm->semaphore) == -1){
            fprintf(stderr, "Errore critico nel processo %d\n", getpid());
            send_message(client_fd, "CRITICAL_ERROR WITH SHARED_MEMORY\n");
            return;
        }

        for (int i = 0; i < MAX_REQUESTS; i++) {
            if (shm->requests[i].status == REQ_EMPTY) {
                req_idx = i;
                break;
            }
        }

        int my_req_id = -1;

        //if we found the target user and we have a slot for the request, we can create it
        if (target_pid != -1 && req_idx != -1) {

            my_req_id = shm->global_id_counter++;

            shm->requests[req_idx].id = my_req_id;

            strcpy(shm->requests[req_idx].sender, s->username);
            strcpy(shm->requests[req_idx].receiver, target_user);
            strcpy(shm->requests[req_idx].filename, filename);
            strcpy(shm->requests[req_idx].full_path, full_path);
            shm->requests[req_idx].status = REQ_PENDING;
            shm->requests[req_idx].outcome = 0;
        } else {
            send_message(client_fd, "Error: Cannot process transfer request (too many requests).\n");

            if (sem_post(&shm->semaphore) == -1) {
                fprintf(stderr, "Errore critico nel processo %d\n", getpid());
                send_message(client_fd, "CRITICAL_ERROR WITH SHARED_MEMORY\n");
            }

            return;
        }

        if (sem_post(&shm->semaphore) == -1) {
            fprintf(stderr, "Errore critico nel processo %d\n", getpid());
            send_message(client_fd, "CRITICAL_ERROR WITH SHARED MEMORY\n");
            return;
        }



        gid_t original_gid = getegid();

        if (seteuid(0) < 0) {
            perror("Cannot elevate privileges to send signal");
            return;
        }

        printf("Sending signal to target user with PID %d\n", target_pid);

        if (kill(target_pid, SIGUSR1) == -1) {
            perror("Error sending signal to target user");
            send_message(client_fd, "Error: Failed to notify target user.\n");
            return;
        }

        if(seteuid(original_gid) < 0) {
            perror("Error restoring original user ID");
        }

        printf("Signal sent successfully. Waiting for response...\n");

        while(1){
            sem_wait(&shm->semaphore);
            if(shm->requests[req_idx].outcome == 1){
                send_message(client_fd, "Transfer request accepted by target user.\n");
                sem_post(&shm->semaphore);
                break;
            }

            if(shm->requests[req_idx].outcome == 2){
                send_message(client_fd, "Transfer request rejected by target user.\n");
                sem_post(&shm->semaphore);
                break;
            }
            sem_post(&shm->semaphore);
            sleep(1);
        }

    }


        
int notify_transfer_requests(int client_fd, SharedMemory *shm, Session *s){

    int id=-1;
    sem_wait(&shm->semaphore);
    for (int i = 0; i < MAX_REQUESTS; i++) {
        if (shm->requests[i].status == REQ_PENDING && strcmp(shm->requests[i].receiver, s->username) == 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Received transfer request with id %d for file: %s from %s\n", shm->requests[i].id, shm->requests[i].filename, shm->requests[i].sender);
            send_message(client_fd, msg);
            id = i;
        }
    }
    sem_post(&shm->semaphore);
    return id;
}

void accept_transfer_request(int client_fd, char* buffer, SharedMemory *shm, Session *s){

    char filename[64];
    char full_path[PATH_MAX];
    char sender[64];
    char directory[64];
    char transfer_id[64];
    int found=0;

    if (sscanf(buffer, "accept %255s %255s", directory, transfer_id) != 2) {
        char *msg = "ERROR: Invalid format. Use: accept_transfer <sender user> <filename>\n";
        send_message(client_fd, msg);
        return;
    }

    sem_wait(&shm->semaphore);

    for (int i = 0; i < MAX_REQUESTS; i++) {
        if (shm->requests[i].status == REQ_PENDING && strcmp(shm->requests[i].receiver, s->username) == 0 && shm->requests[i].id == atoi(transfer_id)) {
            
 
            strcpy(filename, shm->requests[i].filename);
            strcpy(sender, shm->requests[i].sender);
            strcpy(full_path, shm->requests[i].full_path);
            shm->requests[i].status = REQ_EMPTY;
            shm->requests[i].outcome = 1; //accepted
            found=1;
            break;
        }
    }

    sem_post(&shm->semaphore);

    if (!found) {
        char *msg = "ERROR: Transfer request not found or already accepted.\n";
        send_message(client_fd, msg);
        return;
    }



    FILE *src = fopen(full_path, "rb");
    if(errno== EACCES){
        send_message(client_fd, "Error: Permission denied");
    }else if (!src) {
        send_message(client_fd, "Error: Source file not found.\n");
        return;
    }

    char resolved_path[PATH_MAX];
    snprintf(resolved_path, sizeof(resolved_path), "%s/%s", directory, filename);

    FILE *dest = fopen(resolved_path, "wb");
    if (!dest) {
        fclose(src);
        send_message(client_fd, "Error: Cannot write destination file.\n");
        return;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        fwrite(buf, 1, n, dest);
    }

    fclose(src);
    fclose(dest);

    
    send_message(client_fd, COLOR_GREEN"File transfer completed successfully.\n"COLOR_RESET);
    printf("File transfer from %s to %s completed successfully.\n", sender, s->username);



}
