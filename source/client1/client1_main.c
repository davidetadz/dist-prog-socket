#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../errlib.h"
#include "../sockwrap.h"

#define BUF_SIZE 1024
#define PROT_REQ "GET "
#define PROT_REQ_LEN 4

#define PROT_REQ_END "\r\n"
#define PROT_REQ_END_LEN 2

#define PROT_RES "+OK\r\n"
#define PROT_RES_LEN 5

#define PROT_ERR "-ERR\r\n"
#define PROT_ERR_LEN 6

char *prog_name;
char *read_buf = NULL;
FILE *f = NULL;

void sigHandler(int sig_n) {
	signal(sig_n, sigHandler);

	#if DEBUG
		printf("warn: signal %d captured. Terminating...\n", sig_n);
		fflush(stdout);
	#endif

	exit(-1);
}

int main (int argc, char *argv[])
{
	/* Declare local variables */
	int sock_fd;
	uint16_t port;
	struct in_addr saddr;
	struct sockaddr_in server_addr;

	fd_set s_set;

	struct timeval timeout;

	char res_buf[5];

	int res, i;
	uint32_t filesize, timestamp;

	/* Neeeded by error library */
	prog_name = argv[0];

	// Instantiate signal handlers
	signal(SIGINT, sigHandler);
	signal(SIGTERM, sigHandler);
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

	res = connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	if (res == -1) {
		printf("error: connect failed - %s\n", strerror(errno));
		return -1;
	}

	#if DEBUG
		printf("info: socket connect ok\n");
	#endif

	// Set socket timeout and check correctness
	timeout.tv_sec = 15;
	timeout.tv_usec = 0;
	if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		#if DEBUG
			printf("error: Failed setting receive timeout\n");
		#endif
	}

	if (setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		#if DEBUG
			printf("error: Failed setting send timeout\n");
		#endif
	}

	#if DEBUG
		printf("info: socket timeout set\n");
	#endif

	// Send request for each file
	for (i = 0; i < (argc - 3); i++) {

		char *filename = argv[i + 3];

		/* Request format is |GET| |<filename>|\r|\n| */

		// Send the first part of protocol request and check if it's successful
		if (sendn(sock_fd, PROT_REQ, PROT_REQ_LEN, 0) != PROT_REQ_LEN)
		{
			#if DEBUG
				printf("error: cannot send request header to client\n");
			#endif

			Close(sock_fd);
			exit(-1);
		}

		// Send filename and check if it's successful
		if (sendn(sock_fd, filename, strlen(filename), 0) != strlen(filename))
		{
			#if DEBUG
				printf("error: cannot send filename number %d to client\n", i+1);
			#endif

			Close(sock_fd);
			exit(-1);
		}

		// Send the last part of protocol request and check if it's successful
		if (sendn(sock_fd, PROT_REQ_END, PROT_REQ_END_LEN, 0) != PROT_REQ_END_LEN)
		{
			#if DEBUG
			printf("error: cannot send request closing header to client\n");
			#endif

			Close(sock_fd);
			exit(-1);
		}

		#if DEBUG
		printf("info: request %d sent\n", i);
		#endif

		/* Response if successful is |+OK|\r|\n|B1|B2|B3|B4|<file content>|T1|T2|T3|T4|
		   	where B is size and T is timestamp */

		/* We should wait until there's a response available, or timeout expires */

		// Clear current set of file descriptors
		FD_ZERO(&s_set);

		// Add socket to set
		FD_SET(sock_fd, &s_set);

		// Reset timeout
		timeout.tv_sec = 15;
		timeout.tv_usec = 0;

		#if DEBUG
		printf("info: select()\n");
		#endif

		res = select(FD_SETSIZE, &s_set, NULL, NULL, &timeout);

		#if DEBUG
		printf("info: select returned\n");
		#endif

		if (res == -1) {

			#if DEBUG
			printf("error: select failed, closing\n");
			#endif

			Close(sock_fd);
			exit(-1);

		} else if (res > 0) {

			if (readn(sock_fd, res_buf, 5) != 5) {

				#if DEBUG
				printf("error: cannot read response from server or error received, closing\n");
				#endif

				Close(sock_fd);
				exit(-1);
			} else {

				#if DEBUG
				printf("info: read 5 chars\n");
				#endif

				// Check if response is OK
				if (strcmp(PROT_RES, res_buf) != 0) {
					#if DEBUG
					printf("error: error received, closing\n");
					#endif

					Close(sock_fd);
					exit(-1);
				}

				#if DEBUG
				printf("info: response OK\n");
				#endif

				// Read filesize
				if (readn(sock_fd, &filesize, 4) != 4)
				{
					#if DEBUG
					printf("error: error reading filesize, closing\n");
					#endif

					Close(sock_fd);
					exit(-1);
				}

				filesize = ntohl(filesize);

				#if DEBUG
				printf("info: filesize ok %d\n", filesize);
				#endif

				// Open file and read (receive) file content
				#if DEBUG
				printf("info: receiving file\n");
				#endif

				f = fopen(filename, "w");

				if (f == NULL) {
					#if DEBUG
					printf("error: cannot create file for write\n");
					#endif
				}

				uint32_t total_bytes_received = 0, bytes_received = 0, remaining_bytes = 0;
				read_buf = (char *)malloc(BUF_SIZE * sizeof(char));

				do {
					if ((filesize - total_bytes_received) < BUF_SIZE) {
						remaining_bytes = filesize - total_bytes_received;

						bytes_received = readn(sock_fd, read_buf, remaining_bytes);

						// Error receiving file
						if(bytes_received == -1)
							break;

						// Write buffer to file
						if (fwrite(read_buf, sizeof(char), remaining_bytes, f) < remaining_bytes)
						{
							// Error writing to file
							break;
						}

						total_bytes_received += bytes_received;
						
						printf("TOTAL %d RECEIVED %d\n", filesize, total_bytes_received);
					}
					else
					{
						bytes_received = readn(sock_fd, read_buf, BUF_SIZE);

						// Error receiving file
						if (bytes_received == -1)
							break;

						// Write buffer to file
						if (fwrite(read_buf, sizeof(char), BUF_SIZE, f) < BUF_SIZE)
						{
							// Error writing to file
							break;
						}

						total_bytes_received += bytes_received;

						printf("TOTAL %d RECEIVED %d\n", filesize, total_bytes_received);
					}
				} while (bytes_received > 0);

				// Check if file received successful
				if (total_bytes_received != filesize) {
					#if DEBUG
					printf("error: file not received correctly\n");
					#endif

					free(read_buf);
					read_buf = NULL;
					fclose(f);
					f = NULL;

					// Try to delete uncompleted file transfer
					remove(filename);

					Close(sock_fd);

					exit(-1);
				}

				fclose(f);
				f = NULL;

				free(read_buf);
				read_buf = NULL;

				// Receive timestamp and print file details

				if (readn(sock_fd, &timestamp, 4) != 4)
				{
					#if DEBUG
					printf("error: error reading timestamp, closing\n");
					#endif

					Close(sock_fd);
					exit(-1);
				}

				timestamp = htonl(timestamp);

				#if DEBUG
				printf("info: timestamp %d ok\n", timestamp);
				#endif

				printf("Received file %s\n", filename);
				printf("Received file size %d\n", filesize);
				printf("Received file timestamp %d\n", timestamp);
			}

		} else {
			// Timeout expired
			#if DEBUG
			printf("error: timeout expired while waiting response from server, closing\n");
			#endif

			Close(sock_fd);
			exit(-1);
		}
		
	}

	return 0;
}
