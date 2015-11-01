#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include "status_t.h"
#include "string_t.h"

#define ARGC 3

#define MAX_USERS 30

status_t parse_command_line(int argc, char *argv[], uint16_t *port);

void *client_handler(void *void_args);

int main(int argc, char *argv[])
{
	status_t error;
	
	uint16_t port;
	error = parse_command_line(argc, argv, &port);
	if (error)
	{
		goto exit0;
	}

	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
	{
		error = SOCKET_OPEN_ERROR;
		goto exit0;
	}

	struct sockaddr_in sad;
	memset(&sad, 0, sizeof sad);
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = INADDR_ANY;
	sad.sin_port = htons(port);

	if (bind(listen_sock, (struct sockaddr *) &sad, sizeof sad) < 0)
	{
		error = BIND_ERROR;
		goto exit1;
	}

	if (listen(listen_sock, MAX_USERS) < 0)
	{
		error = LISTEN_ERROR;
		goto exit1;
	}

	struct sockaddr_in cad;
	socklen_t clilen = sizeof cad;

	while (1)
	{
		int connection_sock = accept(listen_sock, (struct sockaddr *) &cad, &clilen);
		if (connection_sock < 0)
		{
			print_error_message(ACCEPT_ERROR);
			close(connection_sock);
		}
		else
		{
			pthread_t thread;
			if (pthread_create(&thread, NULL, client_handler, NULL) != 0)
			{
				print_error_message(PTHREAD_CREATE_ERROR);
			}
		}

	}

exit1:
	close(listen_sock);
exit0:
	print_error_message(error);
	return error;
}

status_t parse_command_line(int argc, char *argv[], uint16_t *port)
{
	if (argc != ARGC)
	{
		printf("Usage: ftpserver logfile port\n");
		return BAD_COMMAND_LINE;
	}

	int tmp = atoi(argv[2]);
	if (tmp <= 0 || tmp > UINT16_MAX)
	{
		printf("Port number must be positive and less than %u.\n", UINT16_MAX);
		return BAD_COMMAND_LINE;
	}
	*port = tmp;

	return SUCCESS;
}

void *client_handler(void *void_args)
{
	printf("hi!\n");
}
