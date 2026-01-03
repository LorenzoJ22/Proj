#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "system_ops.h"
#include "values.h"
#include "session.h"

int ensure_user_exists(const char *username) {

    struct passwd *user= getpwnam(username);

   return (user != NULL);
}

int sys_create_user(const char *username) {

    pid_t pid = fork();

    if (pid < 0) {
        // error handling
        perror("Error forking process to create group");
        return -1;
    }

    if (pid == 0) {
        execlp("adduser", "adduser", "--disabled-password","--no-create-home","--gecos" ,"","--ingroup", GROUP_NAME, username, NULL);
    perror("exec adduser");
    _exit(1);
    }

    waitpid(pid, NULL, 0);
    return 1;
}

void sys_make_directory(const char *path, mode_t mode, const char *groupname, const char *username) {

    struct group *grp = getgrnam(groupname);
    struct passwd *pwd = getpwnam(username);
    if (grp == NULL || pwd == NULL) {
        perror("Error: Group or user not found");
        exit(1);
    }

    gid_t new_gid = grp->gr_gid;


    struct stat st;
    if (stat(path, &st) == -1) {
        //umask(002); // clear umask to set exact permissions
        printf("Creating directory at %s\n", path);
        if (mkdir(path, mode) < 0) {
            perror("Failed to create directory");
            exit(1);
        }
    }
        printf("Directory already created\n");

    if (chown(path, pwd->pw_uid, new_gid) == -1) {
        perror("Error changing group ownership");
        exit(1);
    }

    //umask(022); // reset umask

    
}

int sys_make_file(const char *path, mode_t mode, const char *groupname, const char *username) {

    struct group *grp = getgrnam(groupname);
    struct passwd *pwd = getpwnam(username);
    if (grp == NULL || pwd == NULL) {
        perror("Error: Group or user not found");
        exit(1);
    }

    gid_t new_gid = grp->gr_gid;

    //struct stat st;
    //if (stat(path, &st) == -1) { //it is not necessary until we handle this case in the errno of the open function
        umask(002); // clear umask to set exact permissions
        printf("Creating file at %s\n", path);
        int fd;
        if ((fd = open(path, O_CREAT | O_RDWR | O_EXCL, mode)) < 0) {
        if (errno == EEXIST) {
            fprintf(stderr, "Error: file '%s' already exists.\n", path);
            return -errno;//alternative we can pass the client file in the sys_make_file in order to write message of warnings to client shell like "file already exists" 
        } 
        else if (errno == EACCES) {
            fprintf(stderr, "Error: Permession denied to create file at '%s'.\n", path);
            //exit(1);
            return -errno;
        }else{
            perror("Failed to create file"); 
            exit(1);
            }
        } 
        close(fd); 
        printf("File created with permission \n.");
    //}

    //printf("File is already created\n");

    if (chown(path, pwd->pw_uid, new_gid) == -1) {
        perror("Error changing group ownership");
        exit(1);
    }

    umask(022); // reset umask

    return 0;
}

//for now is not available this function
/* void change_dir(const char *path, int client_fd, Session *s){

    char msg[PATH_MAX + 100];

    if(chdir(path)!=0){
        //it failed, we have to report the error
        printf("Command cd failed at %s\n", path);
        snprintf(msg, sizeof(msg), "cd failed: %s\n", strerror(errno));
        write(client_fd, msg, strlen(msg));
    }else{
        //Directory updated correctly
        //update the session current_dir
       if (getcwd(s->current_dir, sizeof(s->current_dir)) != NULL) {
            printf("Directory changed at: %s\n", s->current_dir);
            dprintf(client_fd, "Your directory has changed: %s\n", s->current_dir);
        } else {
            printf("Error cwd not found at %s\n", path);
            dprintf(client_fd, "Critic error: impossible to get CWD .\n");
        }
    }
} */


int create_group(const char *groupname) {

    struct group *g = getgrnam(groupname);
    if (g != NULL) return 1; // group exists

    pid_t pid = fork();

    if (pid < 0) {
        // error handling
        perror("Error forking process to create group");
        return -1;
    }

    if (pid == 0) {
        execlp("groupadd", "groupadd", groupname, (char*)NULL);
        perror("exec groupadd");
        exit(1);
    }

    waitpid(pid, NULL, 0);
    return 1;
}


 void check_full_path(int client_fd, char *path, Session *s, char *full_path){

        int is_absolute = 0;
            if (strncmp(path,s->root_dir, strlen(s->root_dir)) == 0) {
                if ( path[strlen(s->root_dir)] == '/') {
                is_absolute = 1;
               }
            }
               
        size_t needed_len = strlen(s->current_dir) + strlen(path) + 2; 
               
        //char full_path[PATH_MAX + 1000];//il full_path inizializzato con 4096 ha la stessa misura del current_dir quindi nel peggiore dei casi con snprintf la tronca uscendo dalla stringa
               /*^ we have increase this parameter to not have warnings^*/
               if (is_absolute) {
                   //CASE ABSOLUTE: path already begin with "root/..."
                   if (strlen(path) >= PATH_MAX) {
                       char msg[] = "Error: Path too long\n";
                       write(client_fd, msg, strlen(msg));
                       //memset(buffer, 0, sizeof(&buffer));
                       return;
                   }
                   strncpy(full_path, path, PATH_MAX - 1);
               } else {
                //CASE RELATIVE: left the path without change

            if (needed_len > PATH_MAX) {//check if he is arrive to the max 
                char msg[] = "Error: Path too long\n";
                write(client_fd, msg, strlen(msg));
                return;
            }
            snprintf(full_path, PATH_MAX +1000, "%s/%s", s->current_dir, path);
            
        }
        return;
}


void normalize_path(char *path, Session *s) {
    char *stack[100]; // Stack per le parti del percorso
    int top = -1;
    char temp[4096];
    printf("Start nomalizing path\n");
    // Copiamo il path per usare strtok senza distruggere l'originale subito
    strncpy(temp, path, 4096);
    
    char *token = strtok(temp, "/");
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Ignora il "." (resta dove sei)
        } 
        else if (strcmp(token, "..") == 0) {
            // Se c'è qualcosa nello stack, torna indietro
            if (top >= 0) {
                // OPTIONAL: If we want to deniyng delete "root",
                // check: if stack[top] is "root", do not decrease.
                if (strcmp(stack[top], s->username) != 0) {
                     top--; 
                }
            }
        } 

        else if (strlen(token) > 0) { // Avoid void string (caso //)
            // Add directory to the stack
            top++;
            stack[top] = token;
        }
        token = strtok(NULL, "/");
    }

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
}




/* int can_access_dir(char *path) {
    struct stat sb;

    // 1. Recupera le info del file/directory
    if (stat(path, &sb) == -1) {
        perror("stat failed");
        return 0; // Se non esiste o errore, nega accesso
    }

    // 2. Controlla se è una directory
    if (!S_ISDIR(sb.st_mode)) {
        return 0; // Non è una directory, non puoi fare cd
    }

    // 3. Controlla i permessi
    // Qui devi decidere QUALE permesso guardare:
    // S_IXUSR = Owner execute (rwx------)
    // S_IXGRP = Group execute (---rwx---) -> Caso più probabile per un server condiviso
    // S_IXOTH = Others execute (------rwx)

    // Esempio: Controlliamo se il GRUPPO ha il permesso di esecuzione (0010)
    if (sb.st_mode & S_IXGRP) {
        return 1; // Accesso consentito
    }

    return 0; // Accesso negato
} */