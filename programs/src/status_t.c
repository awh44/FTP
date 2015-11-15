#include <stdio.h>

#include "log.h"
#include "status_t.h"

char *get_error_message(status_t error)
{
	switch (error)
	{
		case SUCCESS:
			return "";
		case BAD_COMMAND_LINE:
			//let specific cases handle in main
			return "";
		case FILE_OPEN_ERROR:
			return "Could not open file.";
		case FILE_WRITE_ERROR:
			return "Could not write to log file.";
		case SOCKET_OPEN_ERROR:
			return "Could not open socket.";
		case SOCKET_WRITE_ERROR:
			return "Could not write to socket.";
		case SOCKET_READ_ERROR:
			return "Could not read from socket.";
		case BIND_ERROR:
			return "Could not bind to socket for data connection.";
		case LISTEN_ERROR:
			return "Could not listen on socket for data connection.";
		case ACCEPT_ERROR:
			return "Could not accept connections on the data connection socket.";
		case SOCK_NAME_ERROR:
			return "Could not get port number of data connection socket.";
		case HOST_ERROR:
			return "Could not find the specified host.";
		case MEMORY_ERROR:
			return "Could not allocate memory.";
		case ACCEPTING_ERROR:
		case LOG_IN_ERROR:
		case SERVICE_AVAILIBILITY_ERROR:
			//these three errors come from the server. Outputting the response
			//itself displays an error
			return "";
		case GET_NAME_ERROR:
			//happens only once
			return "";
		case TIME_GET_ERROR:
			return "Could not retreive time for log file.";
		case TIME_STRING_ERROR:
			return "Could not convert time to string for log fie.";
		case NON_FATAL_ERROR:
			//non fatal error - don't need to let user know
			return "A non-fatal error occurred.";
		case PTHREAD_CREATE_ERROR:
			return "Could not create pthtread.";
		case SOCKET_EOF:
			return "Socket end of file reached.";
		case REALPATH_ERROR:
			return "Could not determine path.";
		case CONFIG_FILE_ERROR:
			return "";
		default:
			return "Unknown error";
	}
}

void print_error_message(status_t error)
{
	char *error_message = get_error_message(error);
	if (strcmp(error_message, "") != 0)
		printf("%s\n", error_message);
}

/*
void print_and_log_error_message(status_t error, log_t *log)
{
	char *error_message = get_error_message(error);
	size_t length = strlen(error_message);
	if (length != 0)
	{
		printf("%s\n", error_message);
		write_log(log, error_message, length);
	}
}
*/
