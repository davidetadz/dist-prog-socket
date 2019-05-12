server: server1/server1_main.c errlib.c sockwrap.c
	gcc -std=gnu99 -o server server1/*.c *.c -Iserver1 -lpthread -lm

server_dbg: server1/server1_main.c errlib.c sockwrap.c
	gcc -std=gnu99 -Wall -DDEBUG -o server server1/*.c *.c -Iserver1 -lpthread -lm

client: client1/client1_main.c errlib.c sockwrap.c
	gcc -std=gnu99 -o client client1/*.c *.c -Iclient1 -lpthread -lm

client_dbg: client1/client1_main.c errlib.c sockwrap.c
	gcc -std=gnu99 -Wall -DDEBUG -o client client1/*.c *.c -Iclient1 -lpthread -lm