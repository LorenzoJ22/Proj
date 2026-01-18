#include "client_handler.h"
#include "session.h"
#include "system_ops.h"
#include "values.h"
#include "permissions.h"

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
    if (sscanf(buffer + 5, "%63s", path) != 1) {
        strcpy(path, ".");
    }

    //dprintf(client_fd, "Path richiesto: %s\n", path);

    char full_path[PATH_MAX + 1000];
    
    if (realpath(path, full_path)==NULL) {
        dprintf(client_fd, "Error: Path error\n");
        return;
    }
    //if(check_home_violation(full_path,client_fd, s)==-1)return;

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

    // 5. Preparazione Intestazione
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