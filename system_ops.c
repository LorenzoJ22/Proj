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
#include <dirent.h>


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
        perror(COLOR_RED "Error:"  COLOR_RESET"Group or user not found");
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
            fprintf(stderr,COLOR_RED "Error: Permession denied to create file at '%s'.\n"COLOR_RESET, path);
            //exit(1);
            return -errno;
        }else{
            perror(COLOR_RED"Failed to create file"COLOR_RESET); 
            exit(1);
            }
        } 
        close(fd); 
        printf(COLOR_GREEN"File created with permission \n."COLOR_RESET);
    //}

    //printf("File is already created\n");

    if (chown(path, pwd->pw_uid, new_gid) == -1) {
        perror("Error changing group ownership");
        exit(1);
    }

    umask(022); // reset umask

    return 0;
}



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


int normalize_path(char *path, Session *s, int client_fd) {
    char *stack[100]; // Stack per le parti del percorso
    int top = -1;
    char temp[4096];
    printf("Start nomalizing path\n");
    // Copiamo il path per usare strtok senza distruggere l'originale subito
    strncpy(temp, path, 4096);
    int locked_index=1;
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
    if (top >= 1 && strcmp(stack[1], s->username) != 0) stack[1] = s->username;
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





void move_file(int client_fd, const char *source_path, const char *dest_dir) {
    char full_dest_path[PATH_MAX];
    char *filename;

    // 1. Estraiamo il nome del file dal percorso sorgente
    // Esempio: "home/user/file.txt" -> filename punta a "file.txt"
    filename = strrchr(source_path, '/');
    if (filename == NULL) {
        filename = (char *)source_path; // Case where there is no slash
    } else {
        filename++; // Hop the slash
    }

    // 2. We built the complete path of destination 
    // Es: "home/user/subdir" + "/" + "file.txt"
    // We use snprintf to avoid buffer overflow
    size_t needed = snprintf(full_dest_path, sizeof(full_dest_path), "%s/%s", dest_dir, filename);

    if (needed >= sizeof(full_dest_path)) {
        dprintf(client_fd,"Error: Path too long.\n");
        return;
    }

    // 3. Execute the effective movement 
    if (rename(source_path, full_dest_path) == 0) {
        dprintf(client_fd,COLOR_GREEN"Moved with success from %s to %s\n"COLOR_RESET, source_path, full_dest_path);
        return;
    } else {
        // Management of common error
        if (errno == EXDEV) {
            dprintf(client_fd, COLOR_RED "Error:" COLOR_RESET "Cannot move between disks (copy+delete required).\n");
        } else if (errno == EEXIST || errno == ENOTEMPTY) {
            dprintf(client_fd, COLOR_RED "Error:" COLOR_RESET "The destination file/folder already exists.\n");
        } else if(errno == EACCES || errno == EPERM){
            perror("Error: ");
            dprintf(client_fd,COLOR_RED "Error:" COLOR_RESET "permission denied or he directory containing oldpath has the "); 
            dprintf(client_fd,"sticky  bit (S_ISVTX) set  and  the process's effective user ID is neither the user ID of the file to be ");
            dprintf(client_fd,"deleted nor that of the  directory  containing it,  and the process is not privileged\n");
        } else{
            perror(COLOR_RED"Error:"COLOR_RESET);
            dprintf(client_fd,COLOR_RED"Error rename\n"COLOR_RESET);
        }
        return;
    }
}

// Helper per convertire i flag di mode_t in stringa (es. rwxr-xr-x)
void get_perm_string(mode_t mode, char *str) {
    strcpy(str, "----------"); // 10 chars + null terminator

    // Directory o File?
    if (S_ISDIR(mode))  str[0] = 'd';
    
    // User
    if (mode & S_IRUSR) str[1] = 'r';
    if (mode & S_IWUSR) str[2] = 'w';
    if (mode & S_IXUSR) str[3] = 'x';
    
    // Group
    if (mode & S_IRGRP) str[4] = 'r';
    if (mode & S_IWGRP) str[5] = 'w';
    if (mode & S_IXGRP) str[6] = 'x';
    
    // Others
    if (mode & S_IROTH) str[7] = 'r';
    if (mode & S_IWOTH) str[8] = 'w';
    if (mode & S_IXOTH) str[9] = 'x';
}


// Funzione ricorsiva per calcolare la dimensione totale del contenuto di una directory
long long get_directory_content_size(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0; // Se non possiamo aprire (es. permessi negati), ritorniamo 0

    long long total_size = 0;
    struct dirent *dir;
    char sub_path[PATH_MAX + 1000];
    struct stat st;

    while ((dir = readdir(d)) != NULL) {
        // Saltiamo . e .. per evitare loop infiniti
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        snprintf(sub_path, sizeof(sub_path), "%s/%s", path, dir->d_name);

        if (stat(sub_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Se è una sottocartella, aggiungiamo la sua dimensione (4096) 
                // PIÙ il contenuto ricorsivo della sottocartella stessa
                total_size += st.st_size; 
                total_size += get_directory_content_size(sub_path);
            } else {
                // Se è un file, aggiungiamo la sua dimensione
                total_size += st.st_size;
            }
        }
    }
    closedir(d);
    return total_size;
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