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