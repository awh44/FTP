#include <stdio.h>
#include "status_t.h"

void print_error_message(status_t error)
{
	switch (error)
	{
		case SUCCESS:
			break;
		case BAD_COMMAND_LINE:
			//let specific cases handle in main
			break;
		case FILE_OPEN_ERROR:
			printf("Could not open file for writing.\n");
			break;
		case FILE_WRITE_ERROR:
			printf("Could not write to log file.\n");
			break;
		case SOCKET_OPEN_ERROR:
			printf("Could not open socket.\n");
			break;
		case SOCKET_WRITE_ERROR:
			printf("Could not write to socket.\n");
			break;
		case SOCKET_READ_ERROR:
			printf("Could not read from socket.\n");
			break;
		case BIND_ERROR:
			printf("Could not bind to socket for data connection.\n");
			break;
		case LISTEN_ERROR:
			printf("Could not listen on socket for data connection.\n");
			break;
		case ACCEPT_ERROR:
			printf("Could not accept connections on the data connection socket.\n");
		case SOCK_NAME_ERROR:
			printf("Could not get port number of data connection socket.\n");
			break;
		case HOST_ERROR:
			printf("Could not find the specified host.\n");
			break;
		case MEMORY_ERROR:
			printf("Could not allocate memory.\n");
			break;
		case ACCEPTING_ERROR:
		case LOG_IN_ERROR:
		case SERVICE_AVAILIBILITY_ERROR:
			//these three errors come from the server. Outputting the response
			//itself displays an error
			break;
		case GET_NAME_ERROR:
			//happens only once
			break;
		case TIME_GET_ERROR:
			printf("Could not retreive time for log file.\n");
			break;
		case TIME_STRING_ERROR:
			printf("Could not convert time to string for log fie.\n");
			break;
		case NON_FATAL_ERROR:
			//non fatal error - don't need to let user know
			break;
		case PTHREAD_CREATE_ERROR:
			printf("Could not create pthtread.\n");
			break;
		default:
			printf("Unknown error: %u\n", error);
			break;
	}
}
