#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include "string_t.h"

#define MINIMUM_ARGC 3
#define DEFAULT_COMMAND_PORT 21
#define PORT_DIVISOR 256

#define MAKE_COMMAND_FROM_LITERAL(var, command, str_args)\
	command_t var;\
	var.identifier = command;\
	var.ident_n = sizeof command;\
	var.args = str_args

typedef uint64_t status_t;
#define SUCCESS             0
#define BAD_COMMAND_LINE    1
#define FILE_OPEN_ERROR     2
#define FILE_SEEK_ERROR     3
#define SOCKET_OPEN_ERROR   3
#define SOCKET_CLOSE_ERROR  4
#define SOCKET_WRITE_ERROR  5
#define SOCKET_READ_ERROR   6
#define CONNECTION_ERROR    7
#define READ_ERROR          8
#define ACCEPTING_ERROR     9
#define LOG_IN_ERROR        10
#define SERVICE_AVAILIBILITY_ERROR 11
#define NO_IP_ADDRESSES 12
#define GET_NAME_ERROR 13
#define MEMORY_ERROR 14
#define ARGS_ERROR 15
#define BIND_ERROR 16
#define LISTEN_ERROR 17
#define SOCK_NAME_ERROR 18
#define ACCEPT_ERROR 19
#define NON_FATAL_ERROR 20
#define HOST_ERROR 21

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

typedef struct
{
	int command_socket;
	int data_socket;
	int log_file;
	char *ip4;
	char *ip6;
	uint8_t passive_mode;
	uint8_t extended_mode;
} session_t;

typedef struct
{
	char *identifier;
	size_t ident_n;
	string_t *args;
} command_t;

/**
  * parses the command line, ensuring that it's valid and placing the port
  * number in port, if available.
  * @param argc - the argc received by main
  * @param argv - the argv argument array received by main
  * @param port - out param; the port number to be used for the FTP connection
  */
status_t parse_command_line(int argc, char *argv[], uint16_t *port);

/**
  * makes a connection to the given host on the given port, using socket sock
  * @param sock - out param; the socket over which the connection will be made
  * @param host - the host name to which to connect
  * @param port - the port number to which to connect (still in host order)
  */
status_t make_connection(int *sock, char *host, uint16_t port);

/**
  * opens the file to be ussed for logging at file name filename
  * @param fd       - out param; the file desrcriptor for the log file
  * @param filename - the file name to use for the log file
  */
status_t open_log_file(int *fd, char *filename);

/**
  * gets the IPv4 and IPv6 IP addresses, if available, by walking the ifaddrs
  * list and sets them appropriately in session
  * @param session - session object; ip4 and ip6 pointers will be set upon
  * success
  */
status_t get_ips(session_t *session);

/**
  * helpter function for get_ips
  */
status_t try_set_hostname(struct ifaddrs *ifa, char **hostptr, size_t sockaddr_len,
	char *localhost);
status_t do_session(session_t *sesson);

status_t read_initial_response(session_t *session);
status_t log_in(session_t *session);
status_t cwd_command(session_t *session, string_t *args, size_t length);
status_t cdup_command(session_t *session);
status_t list_command(session_t *session, string_t *args, size_t length);
status_t send_list_command(session_t *session, string_t *args);
status_t pwd_command(session_t *session);
status_t quit_command(session_t *session);
status_t port_command(session_t *session, int *listen_socket);
status_t pasv_command(session_t *session, string_t *host, uint16_t *port);
status_t passive_command(session_t *session);
status_t extended_command(session_t *session);
status_t set_up_listen_socket(session_t *session, int *listen_socket, uint16_t
	*listen_port, int af, char *address);

status_t send_command(session_t *session, command_t *command);
status_t read_entire_response(session_t *sesson, string_t *response);
status_t send_command_read_response(session_t *session, command_t *command, string_t *response);
status_t read_single_line(int socket, string_t *line);
status_t read_remaining_lines(int socket, string_t *response);
status_t read_single_character(int socket, char *c);
status_t read_until_eof(int socket, string_t *response);
uint8_t matches_code(string_t *response, char *code);
uint8_t bool_memcmp(char *s1, char *s2, size_t n);
uint8_t bool_strcmp(char *s1, char *s2);

status_t write_log(session_t *session, char *message, size_t length);

int main(int argc, char *argv[])
{
	status_t error;
	uint16_t port;
	error = parse_command_line(argc, argv, &port);
	if (error)
	{
		goto exit0;
	}

	session_t session;
	error = make_connection(&session.command_socket, argv[1], port);
	if (error)
	{
		printf("Could not connect to the specified host.\n");
		goto exit0;
	}

	error = open_log_file(&session.log_file, argv[2]);
	if (error)
	{
		goto exit1;
	}

	error = get_ips(&session);
	if (error)
	{
		goto exit2;
	}

	//try to default to non-passive mode (i.e., use PORT), but if have neither
	//ip address, need to use passive mode
	if (session.ip4 != NULL && session.ip6 != NULL)
	{
		printf("Starting in active mode.\n");
		session.passive_mode = 0;
	}
	else
	{
		printf("Could not find local IPs. Defaulting to passive mode.\n");
		session.passive_mode = 1;
	}

	session.extended_mode = 0;

	error = do_session(&session);
	if (error)
	{
		goto exit3;
	}

exit3:
	free(session.ip4);
	free(session.ip6);
exit2:
	close(session.log_file);
exit1:
	close(session.command_socket);
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

status_t make_connection(int *sock, char *host_str, uint16_t port)
{
	port = htons(port);
	struct hostent *host = gethostbyname(host_str);
	if (host == NULL)
	{
		return HOST_ERROR;
	}

	if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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

status_t open_log_file(int *fd, char *filename)
{
	if ((*fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0600)) < 0)
	{
		printf("Could not open log file.\n");
		return FILE_OPEN_ERROR;
	}

	return SUCCESS;
}

status_t get_ips(session_t *session)
{
	status_t error = SUCCESS;
	session->ip4 = NULL;
	session->ip6 = NULL;
	
	struct ifaddrs *ifaddr;
	if (getifaddrs(&ifaddr) < 0)
	{
		goto exit0;
	}

	struct ifaddrs *ifa;
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
		{
			continue;
		}

		if (ifa->ifa_addr->sa_family == AF_INET && session->ip4 == NULL)
		{
			error = try_set_hostname(ifa, &session->ip4, sizeof(struct sockaddr_in),
				"127.0.0.1");
			if (error)
			{
				free(session->ip6);
				goto exit1;
			}
		}
		else if (ifa->ifa_addr->sa_family == AF_INET6 && session->ip6 == NULL)
		{
			error = try_set_hostname(ifa, &session->ip6, sizeof(struct sockaddr_in6),
				"::1");
			if (error)
			{
				free(session->ip4);
				goto exit1;
			}
		}
	}

exit1:
	freeifaddrs(ifaddr);
exit0:
	return error;
}

status_t try_set_hostname(struct ifaddrs *ifa, char **hostptr, size_t sockaddr_len, char *localhost)
{
	status_t error = SUCCESS;

	char host[NI_MAXHOST];
	int result = getnameinfo
	(
		ifa->ifa_addr,
		sockaddr_len,
		host,
		NI_MAXHOST,
		NULL,
		0,
		NI_NUMERICHOST
	);

	if (result != 0)
	{
		error = GET_NAME_ERROR;
		goto exit0;
	}

	if (strcmp(host, localhost) != 0)
	{
		*hostptr = malloc(NI_MAXHOST);
		if (*hostptr == NULL)
		{
			error = MEMORY_ERROR;
			goto exit0;
		}
		strcpy(*hostptr, host);
	}

exit0:
	return error;
}

status_t do_session(session_t *session)
{
	status_t error;

	error = read_initial_response(session);
	if (error)
	{
		goto exit0;
	}

	error = log_in(session);
	if (error)
	{
		goto exit0;
	}

	string_t line;
	string_initialize(&line);
	uint8_t quit = 0;
	do
	{
		printf("ftp> ");
		string_getline(&line, stdin);
		string_trim(&line);
		
		size_t array_length;
		string_t *args = string_split_skip_consecutive(&line, ' ',
			&array_length, 1);

		char *c_str = string_c_str(args + 0);

		if (bool_strcmp(c_str, "cd"))
		{
			error = cwd_command(session, args, array_length);
		}
		else if (bool_strcmp(c_str, "cdup"))
		{
			error = cdup_command(session);
		}
		else if (bool_strcmp(c_str, "ls"))
		{
			error = list_command(session, args, array_length);
		}
		else if (bool_strcmp(c_str, "pwd"))
		{
			error = pwd_command(session);
		}
		else if (bool_strcmp(c_str, "quit"))
		{
			error = quit_command(session);
			quit = 1;
		}
		else if (bool_strcmp(c_str, "passive"))
		{
			error = passive_command(session);
		}
		else if (bool_strcmp(c_str, "extended"))
		{
			error = extended_command(session);
		}
		else if (c_str[0] != '\0')
		{
			//'\0' check so user can enter empty lines
			printf("Unrecognized command.\n");
			error = SUCCESS;
		}

		size_t i = 0;
		for (i = 0; i < array_length; i++)
		{
			string_uninitialize(args + i);
		}
		free(args);

	} while (!quit && (!error || error == NON_FATAL_ERROR));

exit1:
	string_uninitialize(&line);
exit0:
	return error;
}

status_t read_initial_response(session_t *session)
{
	status_t error;

	string_t response;
	string_initialize(&response);

	error = read_entire_response(session, &response);
	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, SERVICE_READY_IN))
	{
		char_vector_clear(&response);
		error = read_entire_response(session, &response);
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

status_t log_in(session_t *session)
{
	status_t error;

	string_t line, response;
	string_initialize(&line);
	string_initialize(&response);
	
	printf("Username: ");
	string_getline(&line, stdin);
	MAKE_COMMAND_FROM_LITERAL(username_command, "USER", &line);
	error = send_command_read_response(session, &username_command, &response);
	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, NEED_PASSWORD))
	{
		printf("Password: ");
		string_getline(&line, stdin);
		char_vector_clear(&response);
		MAKE_COMMAND_FROM_LITERAL(password_command, "PASS", &line);
		error = send_command_read_response(session, &password_command, &response);
		if (error)
		{
			goto exit0;
		}
		
		if (matches_code(&response, NOT_IMPLEMENTED_SUPERFLUOUS))
		{
			goto exit0;
		}
	}

	if (!matches_code(&response, USER_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	string_uninitialize(&line);
	return error;
}

status_t cwd_command(session_t *session, string_t *args, size_t array_length)
{
	status_t error = SUCCESS;
	if (array_length < 2)
	{
		printf("Please include the directory to which to switch.\n");
		goto exit0;
	}

	string_t response;
	string_initialize(&response);

	MAKE_COMMAND_FROM_LITERAL(command, "CWD", args + 1);

	error = send_command_read_response(session, &command, &response);
	if (error)
	{
		goto exit1;
	}

	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit1;
	}

	if (!matches_code(&response, FILE_ACTION_COMPLETED))
	{
		error = NON_FATAL_ERROR;
		goto exit1;
	}

exit1:
	string_uninitialize(&response);
exit0:
	return error;
}

status_t cdup_command(session_t *session)
{
	status_t error;

	string_t response;
	string_initialize(&response);

	MAKE_COMMAND_FROM_LITERAL(command, "CDUP", NULL);

	error = send_command_read_response(session, &command, &response);
	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit0;
	}

	if (!matches_code(&response, COMMAND_OKAY))
	{
		error = NON_FATAL_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	return error;
}

status_t list_command(session_t *session, string_t *args, size_t length)
{
	status_t error;

	string_t *final_args = length > 1 ? args + 1 : NULL;
	
	int data_socket; 
	if (!session->passive_mode)
	{
		int listen_socket;
		error = port_command(session, &listen_socket);
		if (error)
		{
			goto exit0;
		}

		error = send_list_command(session, final_args);
		if (error)
		{
			close(listen_socket);
			goto exit0;
		}

		struct sockaddr_in6 cad;
		socklen_t cadlen = sizeof cad;
		data_socket = accept(listen_socket, (struct sockaddr *) &cad, &cadlen);
		if (data_socket < 0)
		{
			close(listen_socket);
			error = ACCEPT_ERROR;
			goto exit1;
		}
	}
	else
	{
		string_t host;
		string_initialize(&host);
		uint16_t port;

		error = pasv_command(session, &host, &port);
		if (error)
		{
			string_uninitialize(&host);
			goto exit0;
		}

		error = make_connection(&data_socket, string_c_str(&host), port);
		if (error)
		{
			string_uninitialize(&host);
			goto exit1;
		}

		string_uninitialize(&host);
		error = send_list_command(session, final_args);
		if (error)
		{
			goto exit1;
		}
	}

	string_t data;
	string_initialize(&data);
	error = read_until_eof(data_socket, &data);
	if (error)
	{
		goto exit2;
	}
	printf("%s", string_c_str(&data));
	
	string_t response;
	string_initialize(&response);
	error = read_entire_response(session, &response);
	if (error)
	{
		goto exit3;
	}

	if (!matches_code(&response, CONNECTION_OPEN_NO_TRANSFER) &&
		!matches_code(&response, CLOSING_DATA_CONNECTION))
	{
		error = NON_FATAL_ERROR;
		goto exit3;
	}

exit3:
	string_uninitialize(&response);
exit2:
	string_uninitialize(&data);
exit1:
	close(data_socket);
exit0:
	return error;
}

status_t send_list_command(session_t *session, string_t *args)
{
	status_t error;

	string_t response;
	string_initialize(&response);

	MAKE_COMMAND_FROM_LITERAL(command, "LIST", args);

	error = send_command_read_response(session, &command, &response);
	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit0;
	}

	if (!matches_code(&response, TRANSFER_STARTING) &&
		!matches_code(&response, FILE_STATUS_OKAY))
	{
		error = NON_FATAL_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	return error;
}

status_t pwd_command(session_t *session)
{
	status_t error;

	string_t response;
	string_initialize(&response);

	MAKE_COMMAND_FROM_LITERAL(command, "PWD", NULL);

	error = send_command_read_response(session, &command, &response);
	if (error)
	{
		goto exit0;
	}

	if (!matches_code(&response, PATH_CREATED))
	{
		error = NON_FATAL_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	return error;
}

status_t port_command(session_t *session, int *listen_socket)
{
	status_t error;

	int af;
	char *address;
	if (session->extended_mode)
	{
		af = AF_INET6;
		address = session->ip6;
	}
	else
	{
		af = AF_INET;
		address = session->ip4;
	}

	uint16_t listen_port;
	error = set_up_listen_socket(session, listen_socket, &listen_port, af,
		address);
	if (error)
	{
		goto exit0;
	}

	string_t args;
	string_initialize(&args);
	
	command_t command;
	if (session->extended_mode)
	{
		string_assign_from_char_array(&args, "|2|");
		string_concatenate_char_array(&args, session->ip6);
		string_concatenate_char_array(&args, "|");
		
		//<= 65535, so can only have up to 5 digits (plus '\0')
		char tmp[6];
		sprintf(tmp, "%u", listen_port);
		string_concatenate_char_array(&args, tmp);
		string_concatenate_char_array(&args, "|");

		MAKE_COMMAND_FROM_LITERAL(tmp_command, "EPRT", &args);
		memcpy(&command, &tmp_command, sizeof command);
	}
	else
	{
		string_assign_from_char_array(&args, session->ip4);
		string_replace(&args, '.', ',');
		string_concatenate_char_array(&args, ",");

		uint8_t port_upper = listen_port / PORT_DIVISOR;
		uint8_t port_lower = listen_port % PORT_DIVISOR;

		//uint8_t <= 255, so can only have up to 3 digits (plus '\0')
		char tmp[4];
		sprintf(tmp, "%u", port_upper);
		string_concatenate_char_array(&args, tmp);
		string_concatenate_char_array(&args, ",");
		sprintf(tmp, "%u", port_lower);
		string_concatenate_char_array(&args, tmp);

		MAKE_COMMAND_FROM_LITERAL(tmp_command, "PORT", &args);
		memcpy(&command, &tmp_command, sizeof command);
	}

	string_t response;
	string_initialize(&response);
	
	error = send_command_read_response(session, &command, &response);
	if (error)
	{
		goto exit1;
	}

	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit1;
	}

	if (!matches_code(&response, COMMAND_OKAY))
	{
		error = NON_FATAL_ERROR;
		goto exit1;
	}

exit1:
	string_uninitialize(&response);
	string_uninitialize(&args);
exit0:
	return error;
}

status_t pasv_command(session_t *session, string_t *host, uint16_t *port)
{
	status_t error;
	MAKE_COMMAND_FROM_LITERAL(command, "PASV", NULL);

	string_t response;
	string_initialize(&response);

	error = send_command_read_response(session, &command, &response);
	if (error)
	{
		goto exit0;
	}

	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit0;
	}

	if (!matches_code(&response, ENTERING_PASSIVE_MODE))
	{
		error = NON_FATAL_ERROR;
		goto exit0;
	}

	size_t len;
	string_t *split = string_split(&response, ',', &len);
	while (char_vector_get(split + 0, 0) != '(' && char_vector_get(split + 0, 0) != '=')
	{
		char_vector_remove(split + 0, 0);
	}
	char_vector_remove(split + 0, 0);

	size_t i;
	for (i = 0; i < 4; i++)
	{
		string_concatenate(host, split + i);
		string_concatenate_char_array(host, ".");
		string_uninitialize(split + i);
	}
	char_vector_pop_back(host);

	//The ')' is safe becase atoi stops at the first non-numeric character
	*port = PORT_DIVISOR * atoi(string_c_str(split + len - 2)) +
		atoi(string_c_str(split + len - 1));
	
	string_uninitialize(split + len - 2);
	string_uninitialize(split + len - 1);
	free(split);

exit0:
	string_uninitialize(&response);
	return error;
}

status_t quit_command(session_t *session)
{
	status_t error;

	string_t response;
	string_initialize(&response);

	MAKE_COMMAND_FROM_LITERAL(command, "QUIT", NULL);

	error = send_command_read_response(session, &command, &response);
	if (error)
	{
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	return error;
}

status_t passive_command(session_t *session)
{
	status_t error = SUCCESS;

	char *word;
	if (session->passive_mode)
	{
		if (session->ip4 == NULL && session->ip6 == NULL)
		{
			printf("No local IP addreses were found, so passive mode cannot be turned off.");
			error = NON_FATAL_ERROR;
			goto exit0;
		}

		word = "off";
		session->passive_mode = 0;
	}
	else
	{
		word = "on";
		session->passive_mode = 1;
	}

	printf("Passive mode is now %s.\n", word);

exit0:
	return error;
}

status_t extended_command(session_t *session)
{
	status_t error = SUCCESS;

	printf("Extended mode is now ");
	if (session->extended_mode)
	{
		printf("off");
		session->extended_mode = 0;
	}
	else
	{
		printf("on");
		session->extended_mode = 1;
	}

	printf(".\n");

	return error;
}

status_t set_up_listen_socket(session_t *session, int *listen_socket, uint16_t
	*listen_port, int af, char *address)
{
	status_t error = SUCCESS;

	if (session->extended_mode)
	{
		af = AF_INET6;
		address = session->ip6;
	}
	else
	{
		af = AF_INET;
		address = session->ip4;
	}

	*listen_socket = socket(af, SOCK_STREAM, 0);
	if (*listen_socket < 0)
	{
		error = SOCKET_OPEN_ERROR;
		goto exit_error0;
	}

	struct sockaddr_in6 sad;
	socklen_t size = sizeof sad;
	memset(&sad, 0, size);
	sad.sin6_family = af;
	inet_pton(af, address, &sad.sin6_addr);

	if (bind(*listen_socket, (struct sockaddr *) &sad, size) < 0)
	{
		error = BIND_ERROR;
		goto exit_error1;
	}

	if (listen(*listen_socket, 1) < 0)
	{
		error = LISTEN_ERROR;
		goto exit_error1;
	}

	struct sockaddr_in port_sad;
	if (getsockname(*listen_socket, (struct sockaddr *) &port_sad, &size) < 0)
	{
		error = SOCK_NAME_ERROR;
		goto exit_error1;
	}

	*listen_port = ntohs(port_sad.sin_port);

	//equivalent to return SUCCESS (and don't close the socket)
	goto exit_success;

exit_error1:
	close(*listen_socket);
exit_error0:
exit_success:
	return error;
}

status_t send_command(session_t *session, command_t *command)
{
	status_t error = SUCCESS;

	string_t command_string;
	string_initialize(&command_string);
	//subtract one because the '\0' is unnecessary
	string_assign_from_char_array_with_size(&command_string,
		command->identifier, command->ident_n - 1);
	if (command->args != NULL)
	{
		string_concatenate_char_array(&command_string, " ");
		string_concatenate(&command_string, command->args);
	}
	string_concatenate_char_array(&command_string, "\r\n");

	if (write(session->command_socket,
	          string_c_str(&command_string),
	          string_length(&command_string)) < 0)
	{
		error = SOCKET_WRITE_ERROR;
		goto exit0;
	}

exit0:
	string_uninitialize(&command_string);
	return error;
}

status_t read_entire_response(session_t *session, string_t *response)
{
	status_t error;

	char c;
	int i;

	error = read_single_line(session->command_socket, response);
	if (error)
	{
		goto exit0;
	}

	if (char_vector_get(response, 3) == '-')
	{
		error = read_remaining_lines(session->command_socket, response);
		if (error)
		{
			goto exit0;
		}
	}

	char *c_str = string_c_str(response);
	printf("%s", c_str);
	
	//error = write_log(session, c_str, string_length(response));

	if (matches_code(response, SERVICE_NOT_AVAILABLE))
	{
		error = SERVICE_AVAILIBILITY_ERROR;
		goto exit0;
	}

exit0:
	return error;
}

status_t send_command_read_response(session_t *session, command_t *command,
	string_t *response)
{
	status_t error;
	error = send_command(session, command);
	if (error)
	{
		goto exit0;
	}

	error = read_entire_response(session, response);
	if (error)
	{
		goto exit0;
	}

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
	         !bool_memcmp(string_c_str(response), line_c_str, 3) ||
			 line_c_str[3] != ' ');

exit0:
	string_uninitialize(&line);
	return error;
}

status_t read_single_character(int socket, char *c)
{
	ssize_t bytes_read;
	if ((bytes_read = read(socket, c, 1)) < 0)
	{
		return SOCKET_READ_ERROR;
	}

	return SUCCESS;
}

status_t read_until_eof(int socket, string_t *response)
{
	status_t error = SUCCESS;
	char buff[512];
	ssize_t bytes_read = read(socket, buff, sizeof buff);
	while (bytes_read > 0)
	{
		string_concatenate_char_array_with_size(response, buff, bytes_read);
		bytes_read = read(socket, buff, sizeof buff);
	}

	if (bytes_read < 0)
	{
		error = SOCKET_READ_ERROR;
		goto exit0;
	}

exit0:
	return error;
}

uint8_t matches_code(string_t *response, char *code)
{
	return bool_memcmp(string_c_str(response), code, 3);
}

uint8_t bool_memcmp(char *s1, char *s2, size_t n)
{
	return memcmp(s1, s2, n) == 0;
}

uint8_t bool_strcmp(char *s1, char *s2)
{
	return strcmp(s1, s2) == 0;
}

/*
status_t write_log(session_t *session, char *message, size_t length)
{
	status_t error;
	time_t time = time(NULL);
	if (time < 0)
	{
		error = TIME_GET_ERROR;
		goto exit0;
	}

	char *time_string = ctime(&time);
	if (time_string == NULL)
	{
		error = TIME_STRING_ERROR;
		goto exit0;
	}

	if (write(

exit0:
	return error;
}
*/
