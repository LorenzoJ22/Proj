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

void login( char *buffer, int client_fd, Session *s){
    
            
             // send login result to client
                char username[64];
                sscanf(buffer + 6, "%63s", username);
                int login_result = session_login(s, username);
            if (login_result == 0) {
                char msg[] = "Login successful\n";
                write(client_fd, msg, strlen(msg));
                memset(buffer, 0, sizeof(&buffer));
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
            char msg[] = "Use: create_user <username> <permissions>\n";
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
                char msg[] = "Use: create [-d] <path> <permissions>\n";
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
                char msg[] = "Error: Cannot name a file or directory '.' or '..'\n";
                write(client_fd, msg, strlen(msg));
                return;
            }

            //check if we are triyng to create file in root directory
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
                    
                    char msg[] = "Error: Permission denied. Cannot create files directly in root.\n";
                    write(client_fd, msg, strlen(msg));
                    return;
                }
            }

            mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t
            char full_path[PATH_MAX + 1000];//il full_path inizializzato con 4096 ha la stessa misura del current_dir quindi nel peggiore dei casi con snprintf la tronca uscendo dalla stringa
            /*^ we have increase this parameter to not have warnings^*/

            check_full_path(client_fd, path, s, full_path);
            normalize_path(full_path, s);

            if(is_dir){
                sys_make_directory(full_path, perms, GROUP_NAME,s->username);  
                char msg[] = "Directory created successfully\n";
                write(client_fd, msg, strlen(msg));
                return;
            }
            else{                
                printf("Full path: %s\n", full_path);
                
            int result = sys_make_file(full_path, perms, GROUP_NAME, s->username);  
                printf("Errno number: %d\n", EEXIST);
                if(result == -EEXIST){
                    char msg[] = "File already created\n";
                    write(client_fd, msg, strlen(msg));
                    return;
                }else if(result == -EACCES){
                    char msg[] = "Permission denied\n";
                    write(client_fd, msg, strlen(msg));
                    return;}
                else{
                    char msg[] = "File created successfully\n";
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


void change_directory(int client_fd, char *buffer, Session *s) {
    
    if (!(s->logged_in)) {
            char msg[] = "Cannot create file while you are guest\n";
            write(client_fd, msg, strlen(msg));
            return;
        }
    
   /*  char *args = buffer + 3; 
    
    args[strcspn(args, "\n")] = 0;

    char path[64];
    
    if (sscanf(args, "%63s ", path) != 1) {
                char msg[] = "Use: cd <path>\n";
                write(client_fd, msg, strlen(msg));
                return;
            } */

   
    char *command;
    char *path;

    command = strtok(buffer, " \t\n");
    path = strtok(NULL, " \t\n");
    dprintf(client_fd, "Your command is: %s\n", command);
    //check if the command is correct, although we already check before in the client_handler.c ...
    if(strncmp(command, "cd", 2)!=0){
        dprintf(client_fd, "Wrong command!\n");
        return;
    }
    
    dprintf(client_fd, "Your path passed is: %s\n", path);
    dprintf(client_fd, "Your home directory in general is: %s\n", s->home_dir);
    dprintf(client_fd, "Your current directory is: %s\n", s->current_dir);
    
    //the case where the user pass nothing to cd
    if(path==NULL){
        path= s->home_dir;
        if (path == NULL) {
            dprintf(client_fd, "Error:  There's no 'home' defined.\n");
            return;
        }
    }
    
    char full_path[PATH_MAX *2]; 
    
    /* int is_absolute = 0;
   //if begin with root, it's absolute in ours VFS
    if (strncmp(path, s->root_dir, strlen(s->root_dir)) == 0 && (path[strlen(s->root_dir)] == '\0' || path[strlen(s->root_dir)] == '/')) {
        is_absolute = 1;
    }

    if (is_absolute) {
        snprintf(full_path, sizeof(full_path), "%s", path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", s->current_dir, path);
    } */

    check_full_path(client_fd,path,s,full_path);
    normalize_path(full_path, s);
    
    if (strncmp(full_path, s->root_dir, strlen(s->root_dir)) != 0) {
        char msg[] = "Error: Cannot go outside root directory\n";
        write(client_fd, msg, strlen(msg));
        //actually we already check this in the funcion "normalize_path"
        return;
    }

    //check if the directory where we want to move exist or not 
    struct stat sb;
    
    if (stat(full_path, &sb) == 0 && S_ISDIR(sb.st_mode)  && access(full_path, X_OK) == 0) {
        
        // SUCCESS: directory exist.
        // Update the session with the user
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
                char msg[] = "Use: chmod <path> <permissions>\n";
                write(client_fd, msg, strlen(msg));
                return;
        }

        mode_t perms = strtol(perm_str, NULL, 8); // convert permissions string to mode_t
        
        char full_path[PATH_MAX + 1000];
        check_full_path(client_fd, path, s, full_path);//Function that checks whether the passed path is relative or absolute and also checks the length
        normalize_path(full_path, s);

        if(chmod(full_path, perms)!=0){
            perror("Error with chmod\n");
            dprintf(client_fd, "Error with chmod\n");
            return;
        }
        printf("Permission changed correctly\n");
        dprintf(client_fd,"Permission changed correctly\n");
}


/* void move_file(const char *source_path, const char *dest_dir) {
    char full_dest_path[PATH_MAX];
    char *filename;

    // 1. Estraiamo il nome del file dal percorso sorgente
    // Esempio: "home/user/file.txt" -> filename punta a "file.txt"
    filename = strrchr(source_path, '/');
    if (filename == NULL) {
        filename = (char *)source_path; // Caso in cui non ci sono slash
    } else {
        filename++; // Saltiamo lo slash
    }

    // 2. Costruiamo il percorso completo di destinazione
    // Esempio: "home/user/subdir" + "/" + "file.txt"
    // Usiamo snprintf per evitare buffer overflow
    int needed = snprintf(full_dest_path, sizeof(full_dest_path), "%s/%s", dest_dir, filename);

    if (needed >= sizeof(full_dest_path)) {
        printf("Error: Path too long.\n");
        return;
    }

    // 3. Eseguiamo lo spostamento effettivo
    if (rename(source_path, full_dest_path) == 0) {
        printf("Moved with success from %s to %s\n", source_path, full_dest_path);
        return 0;
    } else {
        // Management of common error
        if (errno == EXDEV) {
            printf("Error: Impossibile spostare tra dischi diversi (serve copia+cancella).\n");
        } else if (errno == EEXIST || errno == ENOTEMPTY) {
            printf("Error: Il file/cartella di destinazione esiste già.\n");
        } else {
            perror("Error rename");
        }
        return;
    }
}
 */