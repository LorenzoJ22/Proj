[CSAP_project 2025/2026] - Client/Server System

All'interno della cartella contentente i vari file del progetto vi è uno script Makefile, per compilare interamente il progetto bisogna entrare all'interno della cartella ed eseguire "make".

Questo dovrebbe compilare l'intero progetto, producendo i vari .o e due eseguibili chiamati server e client.

## Avvio server
Il server deve essere avviato con SUDO, per far partire il server basta eseguire l'eseguibile server con questa sintassi:
sudo ./server <directory> <ip> <port>

Nel campo directory bisogna specificare la cartella dove si vuole hostare il virtual file system, in caso la cartella non esista il server la creerà automaticamente

I campi <ip> e <port> specificano a quale indirizzo ip e a quale porta i client dovrannno connettersi per comunicare con il server, se non vengono specificati il server si avvierà con valori 127.0.0.1 8080

### Output avvio server
Se il server parte correttamente dovrebbe stampare alcuni messaggi di notifica riguardo la creazione della cartella e la connessione instaurata stampando ip e port, rimanendo in un loop.

### Terminazione Server
Il server può essere terminato digitando ed inviando il comando "exit", una volta inviato vi saranno dei messaggi di notifica riguardo la chiusura del server e la distruzione della shared memory creata allo startup

## Avvio Client
Il client non ha bisogno di essere avviato con sudo, per avviarlo basta eseguire il comando

./client <ip> <port>

Anche qui se i parametri ip e port non sono specificati, tenterà di connettersi con valori 127.0.0.1 8080

### Output avvio client
Se il client si è connesso correttamente dovrebbe stampare dei messaggi di notifica di connessione riuscita e dovrebbe rimanere connesso ad una "shell" chiamata guest, significa che il client si è connesso ma attualmente non essendo loggato risulta come ospite avendo operazioni limitate.

### Terminazione Client
Il client può in qualsiasi momento (tranne quando ci sono processi in background) mandare il comando "exit" per chiudere la connessione col server.

## Comandi del client

Quando un client è connesso ma non ha ancora effettuato login ha solo pochi comandi che può eseguire : login e create_user

### create_user

Il client che attualmente è loggato come guest può creare un nuovo utente mandando il comando 

create_user <username> <permission>

Se scritto sintatticamente corretto, il comando creerà un nuovo utente nel sistema con nome <username> e creerà la home directory di questo utente con permessi <permission>, se il comando sarà scritto in maniera sbagliata o mancheranno dei parametri (tipo non si hanno messo i permessi) il server risponderà con il corretto uso del comando.

Se il comando è stato eseguito correttamente il client verrà notificato della riuscita.

### login

Il guest può mandare il comando

login <username>

per tentare di effettuare il login con utente "username", se questo utente fa parte degli utenti creati col comando **create_user** allora il login andrà a buon fine, se tenterà di loggarsi a qualsiasi altro tipo di utente il login fallirà, in entrambi i casi il client viene notificato con la buona o la cattiva riuscita del comando.

Nel caso il login sia andato a buon fine viene notificato della buona riuscita e la shell cambierà, non si chiamerà più **guest** ma avrà un formato **username@shell:/home_directory**

## I successivi comandi possono essere eseguiti solo da utenti loggati correttamente

## create

Il client può mandare il comando 

create [-d] <path> <permission>

per creare un file o una cartella con permessi "permission"  alla posizione "path". Se il comando viene mandato con parametri mancanti, l'utente viene notificato a riguardo. Se il comando viene eseguito correttamente viene stampato un messaggio di successo

## list

Il client può mandare il comando

list <path>

per stampare a schermo il contenuto della cartella specificata in "path", se non viene specificata nessuna cartella verrà stampata la cartella corrente, mostrando nomi, dimensioni e permessi.

## chmod 

Il client 

chmod <path> <permission>

per modificare i permessi del file "path", se viene mandato il comando con argomenti mancanti viene stampato l'usage. Se eseguito correttamente viene stampato un messaggio di riuscita

## move

Il client può mandare il comando

move <path1> <path2>

per spostare il file "path1" verso la direzione "path2". Se path2 è una cartella il file viene spostato all'interno di quella cartella, se nel path2 viene specificato anche il nome del file, allora questo verrà spostato e rinominato.

## cd 

il client può mandare il comando

cd <path>

per spostarsi tra le varie cartelle, cambiando la cartella corrente con path. Se viene inviato il comando senza specificare path, allora si ritorna alla home directory

## read

il client può mandare il comando

read [-offset] <num> <path>

per poter leggere il contenuto del file "path". Se si manda il comando solo specificando il path verrà mostrato tutto il contenuto del file "path", altrimenti è possibile specificare l'opzione -offset con un valore subito dopo, per poter leggere il file partendo da quel determinato punto.

## write 

il client può mandare il comando

write [-offset] <num> <path>

per scrivere all'interno del file "path". Se il file "path" non esiste, viene creato con permessi 0700. Può essere aggiunto il parametro -offset per specificare da quale punto del file si vuole iniziare a scrivere, se non viene specificato si inizia a scrivere dalla file del file. Per terminare la scrittura del file bisogna scrivere in una riga a capo isolata la parola chiave END tutto maiuscolo ed inviare questa riga, questa parola chiave non viene scritta all'interno del file.

## delete

il client può mandare il comando

delete <path>

per eliminare il file path. Se viene eliminato correttamente viene notificato allo user.

## upload

il client può mandare il comando

upload [-b] <client_path> <server_path>

per mandare un file che il client ha in locale alla posizione "client_path" e farne una copia all'interno del server alla posizione specificata da "server_path". Può essere specificato l'opzione -b per far partire questo comando in modalità background, ovvero il client nel mentre può svolgere tutti gli altri comandi. In entrambi i casi alla fine del caricamento il client viene notificato della buona o cattiva riuscita dell'operazione. Il comando exit fallirà se vi sono comandi in background ancora attivi

## download
il client può mandare il comando

download [-b] <server_path> <client_path>

per scaricare un file presente all'interno del server specificato da "server_path" e tenerselo in locale  alla poszione "client_path". Può essere specificato l'opzione -b per far partire questo comando in modalità background, ovvero il client nel mentre può svolgere tutti gli altri comandi.In entrambi i casi alla fine del download il client viene notificato della buona o cattiva riuscita dell'operazione. Il comando exit fallirà se vi sono comandi in background ancora attivi

## exit

Il client può mandare il comando 

exit

per terminare la connessione in quasi ogni momento, il comando fallirà se vi sono operazioni in background, in questo caso l'utente verrà notificato a riguardo. 


# transfer_request

il client può mandare il comando

transfer_request <file> <dest_user>

per fare una richiesta di transferimento file. L'utente "dest_user" SE online, riceve una notifica di trasferimento file. Non appena riceve questa notifica, il client di "dest_user" può solo accettare o rifiutare il trasferimento, in entrambi i casi il client che ha avviato questa richiesta viene notificato riguardo la decisione di "dest_user". Se l'utente destinatario non è online nel momento in cui viene effettuata la richiesta, il client del mittente rimane bloccato in attesa che il destinatario diventi online.

**Il file che il mittente vuole trasferire si deve trovare nella cartella corrente dove viene inviato il comando transfer_request**

il destinatario può rispondere con

accept <directory> <ID>

per accettare la richiesta, il file del mittente verrà copiato all'interno della directory specificata nella risposta

**Il file che si vuole mandare deve avere i permessi che permettono la copia agli altri membri del gruppo, altrimenti la transfer fallirà**

il destinatario può rispondere con

reject

per rifiutare la richiesta e andare avanti con i suoi comandi. In questo caso il mittente viene notificato a riguardo e ritorna anche lui alla sua shell.

Il terminale è sempre attivo pure se non compare la shell col nome, in caso non comparisse basta inviare un qualsiasi comando e la shell tornerà normale.



