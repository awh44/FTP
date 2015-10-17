#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "string_t.h"

#define MINIMUM_ARGC 3
#define DEFAULT_COMMAND_PORT 21
#define DEFAULT_DATA_PORT 20

#define SEND_LITERAL_COMMAND_READ_RESPONSE(socket, command, args, response)\
	send_command_read_response(socket, command, sizeof(command), args,\
		&response);

#define COMMAND_CONDITIONAL(c_str, length, command)\
	length >= sizeof command - 1 && bool_memcmp(c_str, command, sizeof command - 1)
	

typedef uint64_t status_t;
#define SUCCESS             0
#define BAD_COMMAND_LINE    1
#define SOCKET_OPEN_ERROR   2
#define SOCKET_CLOSE_ERROR  3
#define SOCKET_WRITE_ERROR  4
#define SOCKET_READ_ERROR   5
#define CONNECTION_ERROR    6
#define READ_ERROR          7
#define ACCEPTING_ERROR     8
#define LOG_IN_ERROR        9
#define SERVICE_AVAILIBILITY_ERROR 10

#define RESTART "110"
#define SERVICE_READY_IN "120"
#define TRANSFER_STARTING "125"
#define FILE_STATUS_OKAY "150"
#define COMMAND_OKAY "200"
#define NOT_IMPLEMENTED_SUPERFLUOUS "202"
#define SYSTEM_STATUS "122"
#define DIRECTORY_STATUS "212"
#define FILE_STATUS "213"
#define HELP_MESSAGE "214"
#define SYSTEM_NAME "215"
#define SERVICE_READY "220"
#define CLOSING_CONNECTION "221"
#define CONNECTION_OPEN_NO_TRANSFER "225"
#define CLOSING_DATA_CONNECTION "226"
#define ENTERING_PASSIVE_MODE "227"
#define USER_LOGGED_IN "230"
#define FILE_ACTION_COMPLETED "250"
#define PATH_CREATED "257"
#define NEED_PASSWORD "331"
#define NEED_ACCOUNT "332"
#define PENDING_INFORMATION "350"
#define SERVICE_NOT_AVAILABLE "421"
#define CANT_OPEN_DATA_CONNECTION "425"
#define CONNECTION_CLOSED "426"
#define ACTION_NOT_TAKEN_FILE_UNAVAILABLE1 "450"
#define ACTION_ABORTED_LOCAL_ERROR "451"
#define NOT_TAKEN_INSUFFICIENT_STORAGE "452"
#define COMMAND_UNRECOGNIZED "500"
#define SYNTAX_ERROR "501"
#define NOT_IMPLEMENTED "502"
#define BAD_SEQUENCE "503"
#define NOT_IMPLEMENTED_FOR_PARAMETER "504"
#define NOT_LOGGED_IN "530"
#define NEED_ACCOUNT_FOR_STORING "532"
#define ACTION_NOT_TAKEN_FILE_UNAVAILABLE2 "550"
#define ACTION_ABORTED "551"
#define FILE_ACTION_ABORTED "552"
#define FILE_NAME_NOT_ALLOWED "553"

status_t parse_command_line(int argc, char *argv[], uint16_t *port);
status_t make_connection(int *sock, struct hostent *host, uint16_t port);
status_t do_session(int command_socket, struct hostent *host);

status_t read_initial_response(int command_socket);
status_t log_in(int command_socket);
status_t cwd_command(int command_socket, string_t *line);
status_t cdup_command(int command_socket);
status_t quit_command(int command_socket);

status_t send_command(int socket, char *ident, size_t ident_n, string_t *args);
status_t read_entire_response(int socket, string_t *response);
status_t send_command_read_response(int socket, char *ident, size_t ident_n,
	string_t *args, string_t *response);
status_t read_single_line(int socket, string_t *line);
status_t read_remaining_lines(int socket, string_t *response);
status_t read_single_character(int socket, char *c);
uint8_t matches_code(string_t *response, char *code);
uint8_t bool_memcmp(char *s1, char *s2, size_t n);

int main(int argc, char *argv[])
{
	status_t error;
	uint16_t port;
	error = parse_command_line(argc, argv, &port);
	if (error)
	{
		goto exit0;
	}

	port = htons(port);
	struct hostent *host = gethostbyname(argv[1]);
	int control_socket;

	error = make_connection(&control_socket, host, port);
	if (error)
	{
		goto exit0;
	}

	error = do_session(control_socket, host);
	if (error)
	{
		goto exit1;
	}

exit1:
	close(control_socket);
exit0:
	return error;
}

status_t parse_command_line(int argc, char *argv[], uint16_t *port)
{
	if (argc < MINIMUM_ARGC)
	{
		printf("Usage: ftpclient server logfile [port]\n");
		return BAD_COMMAND_LINE;
	}

	if (argc > MINIMUM_ARGC)
	{
		int tmp = atoi(argv[3]);
		if (tmp < 0)
		{
			printf("Port number must be positive.\n");
			return BAD_COMMAND_LINE;
		}

		*port = tmp;
	}
	else
	{
		*port = 21;
	}

	return SUCCESS;
}

status_t make_connection(int *sock, struct hostent *host, uint16_t port)
{
	if ((*sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		return SOCKET_OPEN_ERROR;
	}

	struct sockaddr_in sad;
	memset(&sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_port = port;
	memcpy(&sad.sin_addr, host->h_addr, host->h_length);

	if (connect(*sock, (struct sockaddr *) &sad, sizeof(sad)) < 0)
	{
		close(*sock);
		return CONNECTION_ERROR;
	}

	return SUCCESS;
}

status_t do_session(int command_socket, struct hostent *host)
{
	status_t error;

	error = read_initial_response(command_socket);
	if (error)
	{
		goto exit0;
	}

	error = log_in(command_socket);
	if (error)
	{
		goto exit0;
	}

	string_t line;
	string_initialize(&line);
	do
	{
		printf("ftp> ");
		string_getline(&line, stdin);
		string_trim(&line);

		char *c_str = string_c_str(&line);
		size_t length = string_length(&line);

		if (COMMAND_CONDITIONAL(c_str, length, "cd "))
		{
			error = cwd_command(command_socket, &line);
		}
		else if (COMMAND_CONDITIONAL(c_str, length, "cdup"))
		{
			error = cdup_command(command_socket);
		}
		else if (COMMAND_CONDITIONAL(c_str, length, "quit"))
		{
			error = quit_command(command_socket);
		}
		else
		{
			printf("Unrecognized command.\n");
		}
	} while (string_compare_char_array(&line, "quit") != 0 || error);

exit1:
	string_uninitialize(&line);
exit0:
	return error;
}

status_t read_initial_response(int command_socket)
{
	status_t error;

	string_t response;
	string_initialize(&response);

	error = read_entire_response(command_socket, &response);
	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, SERVICE_READY_IN))
	{
		char_vector_clear(&response);
		error = read_entire_response(command_socket, &response);
		if (error)
		{
			goto exit0;
		}
	}

	if (!matches_code(&response, SERVICE_READY))
	{
		error = ACCEPTING_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	return error;
}

status_t log_in(int command_socket)
{
	status_t error;

	string_t line, response;
	string_initialize(&line);
	string_initialize(&response);
	
	printf("Username: ");
	string_getline(&line, stdin);
	error = SEND_LITERAL_COMMAND_READ_RESPONSE(command_socket, "USER", &line,
		response);
	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, NEED_PASSWORD))
	{
		printf("Password: ");
		string_getline(&line, stdin);
		char_vector_clear(&response);
		error = SEND_LITERAL_COMMAND_READ_RESPONSE(command_socket, "PASS", &line,
			response);
		if (error)
		{
			goto exit0;
		}
		
		if (matches_code(&response, NOT_IMPLEMENTED_SUPERFLUOUS))
		{
			goto exit0;
		}
	}

	if (matches_code(&response, USER_LOGGED_IN))
	{
		goto exit0;
	}

	error = LOG_IN_ERROR;

exit0:
	string_uninitialize(&response);
	string_uninitialize(&line);
	return error;
}

status_t cwd_command(int command_socket, string_t *line)
{
	status_t error;
	char_vector_remove(line, 0);
	char_vector_remove(line, 0);
	string_trim(line);

	string_t response;
	string_initialize(&response);

	error = SEND_LITERAL_COMMAND_READ_RESPONSE(command_socket, "CWD", line,
		response);
	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	return error;
}

status_t cdup_command(int command_socket)
{
	status_t error;

	string_t empty, response;
	string_initialize(&empty);
	string_initialize(&response);

	error = SEND_LITERAL_COMMAND_READ_RESPONSE(command_socket, "CDUP", &empty,
		response);

	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&empty);
	string_uninitialize(&response);
	return error;
}

status_t quit_command(int command_socket)
{
	status_t error;

	string_t empty, response;
	string_initialize(&empty);
	string_initialize(&response);

	error = SEND_LITERAL_COMMAND_READ_RESPONSE(command_socket, "QUIT", &empty,
		response);

	if (error)
	{
		goto exit0;
	}

exit0:
	string_uninitialize(&empty);
	string_uninitialize(&response);
	return error;
}

status_t send_command(int socket, char *ident, size_t ident_n, string_t *args)
{
	status_t error = SUCCESS;

	string_t command;
	string_initialize(&command);
	//subtract one because the '\0' is unnecessary
	string_assign_from_char_array_with_size(&command, ident, ident_n - 1);
	if (string_length(args) > 0)
	{
		string_concatenate_char_array(&command, " ");
		string_concatenate(&command, args);
	}
	string_concatenate_char_array(&command, "\r\n");

	if (write(socket, string_c_str(&command), string_length(&command)) < 0)
	{
		error = SOCKET_WRITE_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&command);
	return error;
}

status_t read_entire_response(int socket, string_t *response)
{
	status_t error;

	char c;
	int i;

	error = read_single_line(socket, response);
	if (error)
	{
		goto exit0;
	}

	if (char_vector_get(response, 3) == '-')
	{
		error = read_remaining_lines(socket, response);
		if (error)
		{
			goto exit0;
		}
	}

	printf("%s", string_c_str(response));

	if (matches_code(response, SERVICE_NOT_AVAILABLE))
	{
		error = SERVICE_AVAILIBILITY_ERROR;
		goto exit0;
	}

exit0:
	return error;
}

status_t send_command_read_response(int socket, char *ident, size_t ident_n,
	string_t *args, string_t *response)
{
	status_t error;
	error = send_command(socket, ident, ident_n, args);
	if (error)
	{
		goto exit0;
	}

	error = read_entire_response(socket, response);

exit0:
	return error;
}

status_t read_single_line(int socket, string_t *line)
{
	status_t error;
	char c;
	
	error = read_single_character(socket, &c);
	if (error)
	{
		goto exit0;
	}
	char_vector_push_back(line, c);

	size_t last_index = 0;
	do
	{
		error = read_single_character(socket, &c);
		if (error)
		{
			goto exit0;
		}

		char_vector_push_back(line, c);
		last_index++;
	} while (char_vector_get(line, last_index - 1) != '\r' ||
	         char_vector_get(line, last_index) != '\n');

exit0:
	return error;
}

status_t read_remaining_lines(int socket, string_t *response)
{
	status_t error;
	string_t line;
	string_initialize(&line);
	
	char *line_c_str;
	do
	{
		char_vector_clear(&line);
		error = read_single_line(socket, &line);
		if (error)
		{
			goto exit0;
		}
		string_concatenate(response, &line);

		 line_c_str = string_c_str(&line);
	} while (string_length(&line) < 4 ||
	         memcmp(string_c_str(response), line_c_str, 3) != 0 ||
			 line_c_str[3] != ' ');

exit0:
	string_uninitialize(&line);
	return error;
}

status_t read_single_character(int socket, char *c)
{
	ssize_t bytes_read;
	do
	{
		if ((bytes_read = read(socket, c, 1)) < 0)
		{
			return SOCKET_READ_ERROR;
		}

	} while (bytes_read == 0);

	return SUCCESS;
}

uint8_t matches_code(string_t *response, char *code)
{
	return memcmp(string_c_str(response), code, 3) == 0;
}

uint8_t bool_memcmp(char *s1, char *s2, size_t n)
{
	return memcmp(s1, s2, n) == 0;
}
