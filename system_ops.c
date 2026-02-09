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
#include <sys/file.h>

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
//redundand because is for command create without checking the passwd e group
void sys_make_directory_creat(const char *path, mode_t mode) {

    struct stat st;
    if (stat(path, &st) == -1) {
        umask(002); // clear umask to set exact permissions
        printf("Creating directory at %s\n", path);
        if (mkdir(path, mode) < 0) {
            perror("Failed to create directory");
            exit(1);
        }
    }else{
        printf("Directory already created\n");}
        umask(022);
    
}

int sys_make_file(const char *path, mode_t mode) {

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






//function that check if user put in input an absolute path, that try to go above his home directory 
int check_home_violation(char* resolved_path, int client_fd, Session *s){
    char allowed_root[PATH_MAX];
    snprintf(allowed_root, PATH_MAX, "/%s", s->username);

    size_t root_len = strlen(allowed_root);

    if (strncmp(resolved_path, allowed_root, strlen(allowed_root)) != 0) {
        char msg[] = "\033[1;31mError: Access denied (Outside home directory)\033[0m\n"; 
        write(client_fd, msg, strlen(msg));
        return -1;
    }
    if (resolved_path[root_len] != '\0' && resolved_path[root_len] != '/') {
        dprintf(client_fd, COLOR_RED"Error: Access denied. Target is not inside your home.\n"COLOR_RESET);
        return -1;
    }
    return 0;
}



int resolve_safe_create_path(char *raw_input, int client_fd, Session *s) {
    char parent_dir[PATH_MAX];
    char filename_part[64];
    char resolved_parent[PATH_MAX];

    // --- LOGICA DEL COLLEGA (Inizio) ---
    
    // 1. Separazione Directory / File
    char *last_slash = strrchr(raw_input, '/');

    if (last_slash != NULL) {
        size_t parent_len = last_slash - raw_input;
        
        if (parent_len == 0) {
            strcpy(parent_dir, "/");
        } else {
            if (parent_len >= PATH_MAX) {
                dprintf(client_fd, COLOR_RED "Error: Path too long.\n" COLOR_RESET);
                return -1;
            }
            strncpy(parent_dir, raw_input, parent_len);
            parent_dir[parent_len] = '\0';
        }
        // Copiamo il nome file
        strcpy(filename_part, last_slash + 1);
    } else {
        // Nessuno slash: è nella directory corrente
        strcpy(parent_dir, ".");
        strcpy(filename_part, raw_input);
    }

    // 2. Controllo Nome File
    if (strlen(filename_part) == 0) {
        dprintf(client_fd, COLOR_RED "Error: Invalid file name (trailing slash?)\n" COLOR_RESET);
        return -1;
    }
    if (strcmp(filename_part, ".") == 0 || strcmp(filename_part, "..") == 0) {
        dprintf(client_fd, COLOR_RED "Error: Cannot name a file '.' or '..'\n" COLOR_RESET);
        return -1;
    }

    // 3. Risoluzione Cartella Genitore (realpath)
    if (realpath(parent_dir, resolved_parent) == NULL) {
        dprintf(client_fd, COLOR_RED "Error: Destination directory not found (%s)\n" COLOR_RESET, strerror(errno));
        return -1;
    }

    // 4. Controllo Violazione Home (Chiama la funzione del collega)
    // Se la funzione del collega restituisce -1, ha già mandato lei il messaggio d'errore.
    if (check_home_violation(resolved_parent, client_fd, s) == -1) {
        return -1;
    }

    return 0; // Successo
}







//function that reaches the final filename in the path and return the name 
char* get_last(char *path, int client_fd){
    //reach the last name of the path
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
            dprintf(client_fd,"Ecco %s\n", filename);
            return filename;
}




int lock_commands(int file_fd, int client_fd, int is_ex_or_sh, int r_w){//choose, with is = 1 put EX, instead is = 0 put SH.
    int lock;
    if(is_ex_or_sh==1){
        lock = LOCK_EX;
    }else if(is_ex_or_sh == 0){
        lock = LOCK_SH;
    }
     if (flock(file_fd, lock | LOCK_NB) < 0) {
        if (errno == EWOULDBLOCK) {
            printf("Entrato nel errno del block\n");
            // Il file è già usato da un altro client!
            if(r_w==1){//r_w = 1 if you want to send message to read or write, otherwise = 0 if it is a normal command
                write(client_fd, "ERROR_LOCKED", 12);
            }else {
                dprintf(client_fd,COLOR_RED"Error: file locked at the moment\n"COLOR_RESET);
            }
            //send(client_fd, "ERROR_LOCKED", 64, 0);
            close(file_fd);
            return -1;
        }else{
            perror("Error\n");
        }
    }
    printf("File of current process blocked successfully\n");
    return 0;
}

void unlock(int file_fd){
    flock(file_fd, LOCK_UN);
}