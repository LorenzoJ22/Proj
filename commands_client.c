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
            //check if the basename of the path is dot or double dot
            char *filename = strrchr(path, '/'); 

            if (filename == NULL) {
                // Case 1: No slash found (ex. "punto", ".", "..")
                // The filename is the entire path
                filename = path;
            } else {
                // Case 2: Slash found (es. "dir/file", "./file", "dir/.")
                // The filename is the string after the last slash
                filename++; 
            }
        
            // check if the name of the file is prohibited
            if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
                char msg[] = COLOR_RED"Error: Cannot name a file or directory '.' or '..'\n"COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
            }

            //check if we are triyng to create file in root directory although we already check this in normalize_path
            size_t dir_len;
            if (filename != path) { 
                //there's a slash in path
                
                // Let's calculate the length of the parent directory.
                // (filename - 1) is the position of the slash.
                // Subtracting 'path' gives us the length of the string before the slash.
                dir_len = (filename - 1) - path;

                // Strict check:
                // 1. The length of the folder in the path must be equal to the length of the root name.
                // 2. The characters must match exactly.
                if (dir_len == strlen(s->root_dir) && 
                    strncmp(path, s->root_dir, dir_len) == 0) {
                    
                    char msg[] = COLOR_RED "Permission denied. Cannot create files directly in root.\n"COLOR_RESET;
                    write(client_fd, msg, strlen(msg));
                    return;
                }
            }

            mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t
            char full_path[PATH_MAX + 1000];//il full_path inizializzato con 4096 ha la stessa misura del current_dir quindi nel peggiore dei casi con snprintf la tronca uscendo dalla stringa
            /*^ we have increase this parameter to not have warnings^*/

            check_full_path(client_fd, path, s, full_path);
            if(normalize_path(full_path, s,client_fd)==-1){return;}

            if(is_dir){
                sys_make_directory(full_path, perms, GROUP_NAME,s->username);  
                char msg[] = COLOR_GREEN "Directory created successfully\n" COLOR_RESET;
                write(client_fd, msg, strlen(msg));
                return;
            }
            else{                
                printf("Full path: %s\n", full_path);
                
            int result = sys_make_file(full_path, perms, GROUP_NAME, s->username);  
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


/* void cd(int client_fd, char *buffer, Session *s){

    if (!(s->logged_in)) {
        char msg[] = "Cannot create file while you are guest\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    // 1. Parsing of the command line
    char *command;
    char *path;
    //char target_resolved[PATH_MAX+2000];

    command = strtok(buffer, " \t\n");
    path = strtok(NULL, " \t\n");
    dprintf(client_fd, "Your command is: %s\n", command);
    //check if the command is correct, although we already check before in the client_handler.c ...
    if(strncmp(command, "cd", 2)!=0){
        dprintf(client_fd, "Wrong command!\n");
        return;
    }
    //the case where the user pass nothing to cd
    dprintf(client_fd, "Your path passed is: %s\n", path);
    dprintf(client_fd, "Your home directory in general is: %s\n", s->home_dir);
    dprintf(client_fd, "Your current directory is: %s\n", s->current_dir);
    //aggiungere opzione se relativo
    if(path==NULL){
        path = s->home_dir;
        if (path == NULL) {
            dprintf(client_fd, "Error:  There's no 'home' defined.\n");
            return;
        }
    }
   
    //realpath(path, target_resolved) == NULL //problema dei permessi se non ha +w e x non funziona e da sempre null
    dprintf(client_fd, "So the path that we are passing now in function is: %s\n", path);
    char full_path[PATH_MAX + 1000];
    check_full_path(client_fd,path,s,full_path);
    change_dir(full_path,client_fd, s);
} */


/* void change_directory(int client_fd, char *buffer, Session *s) {
    
    if (!(s->logged_in)) {
            char msg[] = "Cannot create file while you are guest\n";
            write(client_fd, msg, strlen(msg));
            return;
        }
    

   
    char *command;
    char *path;

    command = strtok(buffer, " \t\n");
    path = strtok(NULL, " \t\n");
    //dprintf(client_fd, "Your command is: %s\n", command);
    //check if the command is correct, although we already check before in the client_handler.c ...
    if(strncmp(command, "cd", 2)!=0){
        dprintf(client_fd, "Wrong command!\n");
        return;
    }
    
    

    //the case where the user pass nothing to cd
    if(path==NULL){
        path= s->home_dir;
        if (path == NULL) {
            dprintf(client_fd, "Error:  There's no 'home' defined.\n");
            return;
        }
    }
    
    char full_path[PATH_MAX *2]; 
    

    check_full_path(client_fd,path,s,full_path);
    if(normalize_path(full_path, s,client_fd)==-1){return;}

    if (strncmp(full_path, s->root_dir, strlen(s->root_dir)) != 0) {
        char msg[] = "Error: Cannot go outside root directory\n";
        write(client_fd, msg, strlen(msg));
        //actually we already check this in the funcion "normalize_path"
        return;
    }

    //check if the directory where we want to move exist or not and if the current user have the permission to access them
    struct stat sb;
    
    if (stat(full_path, &sb) == 0 && S_ISDIR(sb.st_mode)  && access(full_path, X_OK) == 0) {
        
        // SUCCESS: directory exist.
        // Update the session with the user
        strncpy(s->current_dir, full_path, PATH_MAX - 1);
        s->current_dir[PATH_MAX - 1] = '\0'; 

        printf("User changed dir to: %s\n", s->current_dir);
        //usare chdir per spostare l'intero processo
        //chdir(full_path);//da rivedere!! Prima va messo il chroot 
        char msg[PATH_MAX + 50];
        snprintf(msg, sizeof(msg), "Directory changed to: %s\n", s->current_dir);
        write(client_fd, msg, strlen(msg));

    } else {
        dprintf(client_fd,"access: %d ", access(full_path, X_OK));
        dprintf(client_fd,"S_ISDIR: %d ", S_ISDIR(sb.st_mode));
        dprintf(client_fd,"stat: %d ", stat(full_path, &sb) == 0);
        // FAIL: directory does not exist or is not a directory
        char msg[] = "Error: Directory does not exist or you don't have permission\n";
        write(client_fd, msg, strlen(msg));
    }
} */

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
    // NOTA: In ambiente chroot, la home è spesso "/" o "/home" relativo alla gabbia.
    if (path_arg == NULL) {
        // Se s->home_dir è un path assoluto del sistema reale (es /srv/ftp),
        // dentro il chroot potrebbe non avere senso. 
        // Assumiamo che dentro il chroot la home sia la radice "/" o che s->home_dir sia già adattato.
        //char full[PATH_MAX];
        //snprintf(full, PATH_MAX, "%s/%s", "/",s->username);
        //path_arg = full;  PER ADESSO COMMENTO, MA DA FINIRE!
            
    }

    // 4. Risoluzione del percorso con realpath()
    // realpath prende il path relativo (es. ".." o "foto") e lo risolve 
    // basandosi sulla directory corrente del processo (già chrootato).
    char resolved_path[PATH_MAX];
    char cwd[PATH_MAX];
    dprintf(client_fd,"cwd: %s\n", getcwd(cwd, PATH_MAX));

    // NOTA: realpath restituisce NULL se il percorso non esiste.
    if (realpath(path_arg, resolved_path) == NULL) {
        // Errore: la cartella non esiste o permessi negati nella risoluzione
        char msg[PATH_MAX + 3000];
        snprintf(msg, sizeof(msg), "Error: Cannot resolve path '%s': %s\n", path_arg, strerror(errno));
        write(client_fd, msg, strlen(msg));
        return;
    }
    dprintf(client_fd, "Path reale: %s\n", resolved_path);
    // 5. Spostamento del processo con chdir()
    // Siccome siamo in chroot, non possiamo "uscire" dalla root. 
    // Se resolved_path è valido, possiamo andarci.
    if (chdir(resolved_path) == 0) {
        
        // SUCCESS: Aggiorniamo la sessione
        strncpy(s->current_dir, resolved_path, PATH_MAX - 1);
        s->current_dir[PATH_MAX - 1] = '\0';

        // Feedback lato server
        printf("User %s moved to (chroot): %s\n", s->username, s->current_dir);

        // Feedback al client
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
        check_full_path(client_fd, path, s, full_path);//Function that checks whether the passed path is relative or absolute and also checks the length
        if(normalize_path(full_path, s,client_fd)==-1){return;}

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
                char msg[] = "Use: move <path1> <path2>\n";
                write(client_fd, msg, strlen(msg));
                return;
        }
        dprintf(client_fd,"Il path_src: %s\n", path_src);
        dprintf(client_fd,"Il path_dest: %s\n", path_dest);
    char full_path_src[PATH_MAX+1000];
    char full_path_dest[PATH_MAX+1000];
        //now we have to normalize the two path passed and chek also if they are absolute or relative
        check_full_path(client_fd, path_src, s, full_path_src);
        check_full_path(client_fd, path_dest, s, full_path_dest);
        if(normalize_path(full_path_src,s, client_fd)==-1){return;}
        if(normalize_path(full_path_dest,s,client_fd)==-1){return;}
        dprintf(client_fd,"Il full_path_src: %s\n", full_path_src);
        dprintf(client_fd,"Il full_path_dest: %s\n", full_path_dest);
        //now we can move the file in the directory of the second path
        move_file(client_fd, full_path_src, full_path_dest);

}



int normalize_path_list(char *path, Session *s, int client_fd) {
    char *stack[100]; // Stack per le parti del percorso
    int top = -1;
    char temp[4096];
    printf("Start nomalizing path\n");
    // Copiamo il path per usare strtok senza distruggere l'originale subito
    strncpy(temp, path, 4096);
    int locked_index=0;
    char *token = strtok(temp, "/");
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Ignora il "." (resta dove sei)
        } 
        else if (strcmp(token, "..") == 0) {
            // Se c'è qualcosa nello stack, torna indietro
            if (top >= locked_index) {
                // OPTIONAL: If we want to deniyng delete "root",
                // check: if stack[top] is "root", do not decrease.
                if (strcmp(stack[top], s->username) != 0) {
                     top--; 
                }else {
                    dprintf(client_fd, COLOR_RED"Error you are triyng to access area above user\n"COLOR_RESET);
                    return-1;}
            }
        } 

        else if (strlen(token) > 0) { // Avoid void string (caso //)
            // Add directory to the stack
            printf(" stack[%d] = %s\n",top, stack[top]);
            top++;
            stack[top] = token;
            
        }

        token = strtok(NULL, "/");
    }
    if (top >= 0 && strcmp(stack[0], s->root_dir) != 0) stack[0] = s->root_dir;
    //if (top >= 1 && strcmp(stack[1], s->username) != 0) stack[1] = s->username;
    // Rebuild the clean path
    path[0] = '\0'; // Clear the destination string
    
    for (int i = 0; i <= top; i++) {
        if (i > 0) strcat(path, "/"); // Add slash between the words (but not at beginnig if path does not begin with /)
        strcat(path, stack[i]);
    }

    printf("Finish to normalize the path\n");
    
    if (strlen(path) == 0) {
        strcpy(path, s->root_dir);
    }
    return 0;
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
    
    check_full_path(client_fd, path, s, full_path); 
    if (normalize_path_list(full_path, s, client_fd) == -1) {
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

    // 5. Preparazione Intestazione
    struct dirent *dir;
    struct stat file_stat;
    char line_buffer[2048];                
    char filepath_for_stat[PATH_MAX + 2000]; 
    char perm_str[12];                       

    // Scriviamo l'intestazione nel buffer accumulatore
    snprintf(line_buffer, sizeof(line_buffer), 
        "%-12s %-30s %-15s\n", "PERMISSIONS", "NAME", "TOTAL SIZE");
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


