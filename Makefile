# --- 1. CONFIGURAZIONE COMPILATORE ---
CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

# --- 2. DEFINIZIONE DEI FILE SORGENTE ---

# File CONDIVISI (usati sia da Client che da Server)
# network.c sembra contenere le funzioni base di invio/ricezione socket
COMMON_SRCS = network.c 

# File specifici del SERVER
# server.c: il main del server
# client_handler.c: logica del processo figlio (dopo la fork)
# session.c: gestione login e utenti
# system_ops.c: operazioni su file system (create, delete, ecc.)
SERVER_SRCS = server.c client_handler.c session.c system_ops.c

# File specifici del CLIENT
# client.c: il main e l'interfaccia utente del client
CLIENT_SRCS = client.c

# --- 3. CREAZIONE LISTA OGGETTI (.o) ---
COMMON_OBJS = $(COMMON_SRCS:.c=.o)
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# --- 4. TARGET PRINCIPALE ---
all: server client

# --- 5. COMPILAZIONE DEGLI ESEGUIBILI ---

# L'eseguibile 'server' unisce i file server-specifici + network
server: $(SERVER_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS) $(COMMON_OBJS)

# L'eseguibile 'client' unisce il main del client + network
client: $(CLIENT_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS) $(COMMON_OBJS)

# --- 6. REGOLA GENERICA (da .c a .o) ---
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- 7. PULIZIA ---
clean:
	rm -f *.o server client