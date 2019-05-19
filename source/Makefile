server: server2/server2_main.c errlib.c sockwrap.c
	gcc -std=gnu99 -o server server2/*.c *.c -Iserver2 -lpthread -lm

server_dbg: server2/server2_main.c errlib.c sockwrap.c
	gcc -std=gnu99 -Wall -DDEBUG -ggdb -o server server2/*.c *.c -Iserver2 -lpthread -lm

client: client1/client1_main.c errlib.c sockwrap.c
	gcc -std=gnu99 -o client client1/*.c *.c -Iclient1 -lpthread -lm

client_dbg: client1/client1_main.c errlib.c sockwrap.c
	gcc -std=gnu99 -Wall -DDEBUG -ggdb -o client client1/*.c *.c -Iclient1 -lpthread -lm
