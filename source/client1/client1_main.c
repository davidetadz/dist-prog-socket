#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "../errlib.h"
#include "../sockwrap.h"

char *prog_name;

int main (int argc, char *argv[])
{
	/* Declare local variables */
	int sock_fd;
	uint16_t port;
	struct in_addr saddr;
	struct sockaddr_in server_addr;

	socklen_t server_addrlen = sizeof(server_addr);

	struct timeval timeout;

	int res;

	/* Neeeded by error library */
	prog_name = argv[0];

	// Instantiate signal handlers
	signal(SIGPIPE, SIG_IGN);

	// Check arguments
	if (argc < 4)
	{
		printf("Usage: %s <address> <port> <filename_1> [... <filename_N>]\n", argv[0]);
		return -1;
	}

	port = atoi(argv[2]);

	if (strcmp("localhost", argv[1]) == 0)
		res = inet_aton("127.0.0.1", &saddr);
	else
		res = inet_aton(argv[1], &saddr);

	if (res == 0) {
		printf("error: address not valid - should be dotted decimal format eg. 127.0.0.1\n");
		return -1;
	}

	/* Create socket */
	sock_fd = Socket(AF_INET, SOCK_STREAM, 0);

	#if DEBUG
	printf("info: Socket created %d\n", sock_fd);
#endif

	/* Set address */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = saddr;

	return 0;
}
