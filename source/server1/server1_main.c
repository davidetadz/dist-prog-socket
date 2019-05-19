#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include "../errlib.h"
#include "../sockwrap.h"

#define LISTEN_QUEUE 		15
#define BUF_SIZE 			1024
#define PROT_REQ			"GET "
#define PROT_REQ_LEN		4

#define PROT_RES			"+OK\r\n"
#define PROT_RES_LEN		5

#define PROT_ERR			"-ERR\r\n"
#define PROT_ERR_LEN		6

char *prog_name;
char *read_buf;
char *send_buf;
char *file_name = NULL;

FILE *f;

#if DEBUG
FILE *f_log;
#endif
void sigHandler(int sig_n) {
	signal(sig_n, sigHandler);

	#if DEBUG
		printf("warn: signal %d captured. Terminating...\n", sig_n);
		fflush(stdout);
	#endif

	// Free memory
	free(read_buf);
	read_buf = NULL;
	if (file_name != NULL) {
		free(file_name);
		file_name = NULL;
	}

	if (f != NULL) {
		fclose(f);
		f = NULL;
	}

	if (send_buf != NULL) {
		free(send_buf);
		send_buf = NULL;
	}

	#if DEBUG
		fclose(f_log);
		f_log = NULL;
	#endif

	exit(-1);
}

int readreq	(int s, char *str) {
	ssize_t nread;

	nread = recv(s, str, BUF_SIZE, 0);

	return nread;
}

void senderr(int s) {
	send(s, PROT_ERR, PROT_ERR_LEN, 0);
	return;
}

/**
 * Get the size of a file.
 * @return The filesize, or 0 if the file does not exist.
 */
size_t getFileSize(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) != 0)
	{
		return 0;
	}
	return st.st_size;
}

/**
 * Get last edit of a file.
 * @return Last edit time, or 0 if the file does not exist.
 */
time_t getFileTimestamp(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) != 0)
	{
		return 0;
	}
	return st.st_mtime;
}

int handleRequest(int conn_fd, const char* buf, int read_buf_len) {

    int filename_len;
    uint32_t f_timestamp, f_size;

	/* Check request format */
	if (strncmp(read_buf, PROT_REQ, PROT_REQ_LEN) != 0 ||
		(read_buf[read_buf_len - 2] != '\r') ||
		(read_buf[read_buf_len - 1] != '\n'))
	{
		/* Wrong format */
		senderr(conn_fd);

		Close(conn_fd);
		#if DEBUG
		printf("warning: Format not correct, skipping this request\n");
		fprintf(f_log, "%s", read_buf);
		#endif

		return -1;
	}

	/* Copy filename into another variable */
	filename_len = read_buf_len - (PROT_REQ_LEN + 2);
	file_name = (char *)malloc(sizeof(char) * (filename_len + 1));

	memcpy(file_name, read_buf + PROT_REQ_LEN, filename_len);
	file_name[filename_len] = '\0';

	// Check if filename is valid to prevent going up in directories tree
	if (strstr(file_name, "../") != NULL)
	{
		#if DEBUG
		printf("warning: Filename not valid, skipping this request\n");
		fprintf(f_log, "%s", read_buf);
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		file_name = NULL;
		return -1;
	}

	// Check if file exists
	if (access(file_name, R_OK) == -1)
	{
		#if DEBUG
		printf("warning: File not existing or not readable, skipping this request\n");
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		file_name = NULL;
		return -1;
	}

	// Read file stats
	f_size = getFileSize(file_name);
	f_timestamp = getFileTimestamp(file_name);

	// Check file stats
	if (f_size < 0)
	{
		#if DEBUG
		printf("warning: cannot read file size, skipping this request\n");
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		file_name = NULL;
		return -1;
	}

	if (f_timestamp < 0)
	{
		#if DEBUG
		printf("warning: cannot read last file edit timestamp, skipping this request\n");
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		file_name = NULL;
		return -1;
	}

	#if DEBUG
	printf("info: File \'%s\' OK\n", file_name);
	printf("info: size %d timestamp %d\n", f_size, f_timestamp);
	#endif

	f_size = htonl(f_size);
	f_timestamp = htonl(f_timestamp);

	// Open the file in read mode
	f = fopen(file_name, "r");

	// Check if open successful
	if (f == NULL)
	{
		#if DEBUG
		printf("warning: cannot open file, skipping this request\n");
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		file_name = NULL;
		return -1;
	}

	// Send the first part of protocol response and check if it's successful
	if (sendn(conn_fd, PROT_RES, PROT_RES_LEN, 0) != PROT_RES_LEN)
	{
		#if DEBUG
		printf("error: cannot send response header to client, skipping this request\n");
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		fclose(f);
		f = NULL;
		file_name = NULL;
		return -1;
	}

	// Send file size
	if (sendn(conn_fd, &f_size, 4, 0) != 4)
	{
		#if DEBUG
		printf("error: cannot send file size to client, skipping this request\n");
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		fclose(f);
		f = NULL;
		file_name = NULL;
		return -1;
	}

	// Revert file size to system order
	f_size = ntohl(f_size);

	// Send file content
	//uint32_t bytes_sent = sendfile(conn_fd, fileno(f), NULL, BUF_SIZE);

	#if DEBUG
		printf("info: sending file...\n");
	#endif

	uint32_t total_bytes_sent = 0, bytes_sent = 0, bytes_read = 0;
	send_buf = (char *)malloc(BUF_SIZE * sizeof(char));

	do
	{
		bytes_read = fread(send_buf, sizeof(char), BUF_SIZE, f);
		bytes_sent = sendn(conn_fd, send_buf, bytes_read, 0);

		if (bytes_sent == -1) {
			// Error sending file
			break;
		}

		total_bytes_sent += bytes_sent;

	} while (bytes_sent > 0 && !feof(f));

	// Check if file sent successful
	if (total_bytes_sent != f_size)
	{
		#if DEBUG
		printf("error: while sending file to client! difference %d\n", f_size - total_bytes_sent);
		printf("[%s]\n\n", strerror(errno));
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		fclose(f);
		free(send_buf);
		send_buf = NULL;
		f = NULL;
		file_name = NULL;
		return -1;
	}

	free(send_buf);
	send_buf = NULL;
	fclose(f);
	f = NULL;

	free(file_name);
	file_name = NULL;

	#if DEBUG
	printf("info: file sent successful to client\n");
	#endif

	// Send the last part of protocol response and check if it's successful
	if (sendn(conn_fd, &f_timestamp, 4, 0) != 4)
	{
		#if DEBUG
		printf("error: cannot send timestamp to client, closing socket\n");
		#endif

		senderr(conn_fd);
		Close(conn_fd);
		free(file_name);
		file_name = NULL;
		return -1;
	}

	#if DEBUG
	printf("info: timestamp sent successful to client\n");
	#endif

	return 1;
}

int main (int argc, char *argv[])
{
	/* Declare local variables */
	int listen_fd, conn_fd, read_buf_len;
	struct sockaddr_in server_addr, client_addr;
	short port;

	socklen_t client_addrlen = sizeof(client_addr);

	int res;

	/* Neeeded by error library */
	prog_name = argv[0];

	// Instantiate signal handlers
	signal(SIGINT, sigHandler);
	signal(SIGTERM, sigHandler);
	signal(SIGPIPE, SIG_IGN);

	#if DEBUG
	f_log = fopen("log.txt", "w");
	#endif

	// Check arguments
	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		return -1;
	}

	port = atoi(argv[1]);

	/* Create socket */
	listen_fd = Socket(AF_INET, SOCK_STREAM, 0);

	#if DEBUG
		printf("info: Socket created %d\n", listen_fd);
	#endif

	/* Set addres and bind socket to it */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	Bind(listen_fd, (SA*) &server_addr, sizeof(server_addr));

	#if DEBUG
		printf("info: Socket binded to loopback interface\n");
		printf("listening on %s:%u\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
	#endif

	Listen(listen_fd, LISTEN_QUEUE);

	/* Allocate buffers */
	read_buf = (char *)malloc(BUF_SIZE * sizeof(char));

	while(1) {
		
		#if DEBUG
			printf("info: Waiting for connections...\n");
		#endif

		// TODO: Insert select for non-blocking wait condition
		conn_fd = Accept(listen_fd, (SA *)&client_addr, &client_addrlen);

		// Check accept result

		if (conn_fd < 0) {
			if (INTERRUPTED_BY_SIGNAL ||
				errno == EPROTO || errno == ECONNABORTED ||
				errno == EMFILE || errno == ENFILE ||
				errno == ENOBUFS || errno == ENOMEM) {
				#if DEBUG
					printf("warning: Failed accepting incoming connection, trying again\n");
				#endif

				continue;
			} else {
				#if DEBUG
					printf("error: Failed accepting incoming connection\n");
				#endif

				continue;
			}
		}

		// Set socket timeout and check correctness
		struct timeval timeout;
		timeout.tv_sec = 15;
		timeout.tv_usec = 0;
		if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
			#if DEBUG
			printf("error: Failed setting receive timeout\n");
			#endif
		}

		if (setsockopt(conn_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		{
			#if DEBUG
			printf("error: Failed setting send timeout\n");
			#endif
		}

		#if DEBUG
			printf("info: Incoming connection from %s:%u\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
		#endif

		/* Read file request */
		do {
			read_buf_len = readreq(conn_fd, read_buf);

			if (read_buf_len == 0) {
				Close(conn_fd);
				break;
			} else if (read_buf_len < 0) {

				int error = errno;

				if (error == EWOULDBLOCK) {
					#if DEBUG
						printf("warning: Connection timeout, closing\n");
					#endif
				} else {
					#if DEBUG
						printf("error: Read from socket failed, closing\n");
					#endif
				}

				Close(conn_fd);
				break;
			}

			res = handleRequest(conn_fd, read_buf, read_buf_len);

		} while (read_buf_len > 0 && res == 1);

		//Close(conn_fd);
	}

	return 0;
}
