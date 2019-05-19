# Server TCP Sequenziale

```
server <port>
```
Dopo aver stabilito una connessione con il client, accetta una richiesta di trasferimento e spedisce i files richiesti al client. I files sono quelli accessibili dal server nella sua directory di lavoro.

```
client <address> <port> <file_1> ... <file_n>
```

Dopo aver stabilito la connessione il client richiede il trasferimento dei files. Al termine del trasferimento, salva localmente i files nella propria directory di lavoro e stampa in output un messaggio indicante l'avvenuto trasferimento (i valori sono decimali)
```
nomefile dimensione timestamp_last_edit
```

Eventuali timeout devono essere impostati a 15 secondi.

## Protocollo

### Richiesta file
Per richiedere un file il client invia `GET` seguito da uno spazio ` ` e dal nome del file `filename`, terminati dai caratteri ASCII Carriage Return `CR` e Line Feed `LF`.

### Risposta
Il server risponde inviando `+OK` seguito dai caratteri `CR` e `LF`, dal numero di byte del file richiesto (intero senza segno su 32 bit in network byte order), dai bytes del contenuto richiesto e dal timestamp dell'ultima modifica (UNIX time) come intero senza segno su 32 bit in network byte order.

Il client può richiedere più file usando la stessa connessione TCP inviando più comandi GET uno dopo l'altro.

Quando il client ha finito di spedire i comandi, inizia la procedura per chiudere la connessione; l'ultimo file dovrebbe essere stato trasferito prima che la procedura di chiusura termini.

### Errore
In caso di errore (es. file inesistente) il server risponde con `-ERR` seguito da `CR` e `LF`, e procede a chiudere in modo ordinato la connessione con il client.