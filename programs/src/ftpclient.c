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
#define SUCCESS                     0
#define BAD_COMMAND_LINE            1
#define FILE_OPEN_ERROR             2
#define FILE_WRITE_ERROR            3
#define SOCKET_OPEN_ERROR           4
#define SOCKET_WRITE_ERROR          5
#define SOCKET_READ_ERROR           6
#define CONNECTION_ERROR            7
#define BIND_ERROR                  8
#define LISTEN_ERROR                9
#define ACCEPT_ERROR               10
#define SOCK_NAME_ERROR            11
#define HOST_ERROR                 12
#define MEMORY_ERROR               13
#define ACCEPTING_ERROR            14
#define LOG_IN_ERROR               15
#define SERVICE_AVAILIBILITY_ERROR 16
#define GET_NAME_ERROR             17
#define TIME_GET_ERROR             18
#define TIME_STRING_ERROR          19
#define NON_FATAL_ERROR            20

#define RESTART "110"
#define SERVICE_READY_IN "120"
#define TRANSFER_STARTING "125"
#define FILE_STATUS_OKAY "150"
#define COMMAND_OKAY "200"
#define NOT_IMPLEMENTED_SUPERFLUOUS "202"
#define SYSTEM_STATUS "211"
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

//------------------------------SETUP FUNCTIONS--------------------------------
/**
  * parses the command line, ensuring that it's valid and places the port
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
  * @param session - session object; ip4 and ip6 pointers will be on success
  */
status_t get_ips(session_t *session);

/**
  * helpter function for get_ips; if the host can be retrieved from ifa and does
  * not equal localhost, then *hostptr is malloc'd and set
  * @param ifa       - the ifaddrs object to get the host name from
  * @param hostptr   - pointer to a character pointer which will be malloc'c and
  * 	contain the name of the host upont success
  * @param len       - the size of a sockaddr object
  * @param localhost - the localhost to ignore if the hostname equals
  */
status_t try_set_hostname(struct ifaddrs *ifa, char **hostptr, size_t len,
	char *localhost);

/**
  * actually begins the user's session by reading initial response, logging in,
  * and then continously reading the user's commands and interpreting them
  * @param session - the user's session object, containing necessary info
  */
status_t do_session(session_t *session);
//----------------------------END SETUP FUNCTIONS-------------------------------

//-----------------------------COMMAND FUNCTIONS-------------------------------
/**
  * reads the initial response from the server after connecting
  * @param session - the current session's object
  */
status_t read_initial_response(session_t *session);

/**
  *	completes the log in process on the connection contained in session,
  *	including USER and PASS commands where appropriate
  * @param session - the current session's object
  */
status_t log_in(session_t *session);

/**
  *	sends a CWD command and recognizes any errors that occur, using the command
  *	socket in session and the command line args array of length length
  * @param session - the current session's object
  * @param args    - the arguments the user passed on the command line
  * @param length  - the length of the args array
  */
status_t cwd_command(session_t *session, string_t *args, size_t length);

/**
  * sends a CDUP command and recognizes any errors that occur, using the command
  * socket in session
  */
status_t cdup_command(session_t *session);

/**
* sends a LIST command, including setup with PORT/PASV commands, and
* recognizes any errors that occur. Uses the command socket in session and
* the command line args array of length length
* @param session - the current session's object
* @param args    - the arguments the user passed on the command line
* @param length  - the length of the args array
*/
status_t list_command(session_t *session, string_t *args, size_t length);

/**
  * actually sends the LIST command itself within the current session, using the
  * arguments args
  * @param session - the current session to use
  * @param args    - the args to be passed to the server with the LIST command
  */
status_t send_list_command(session_t *session, string_t *args);

/**
  * sends a RETR command, including setup with PORT/PASV commands, and
  * recognizes any errors that occur. Uses the command socket in session and
  * the command line args array of length length
  * @param session - the current session's object
  * @param args    - the arguments the user passed on the command line
  * @param length  - the length of the args array
  */
status_t retr_command(session_t *session, string_t *args, size_t length);

/**
  * actually sends the RETR command itself within the current session, using the
  * arguments args
  * @param session - the current session to use
  * @param args    - the args to be passed to the server with the LIST command
  */
status_t send_retr_command(session_t *session, string_t *args);

/**
  * given the command line arguments args, with length length, writes the data
  * in data to either the file name provided in args (args[2]) or the file name retrieved
  * from the server, held in args (args[1])
  * @param args   - the args parsed from the command line
  * @param length - the length of the args array
  * @param data   - the data to write to the file
  */
status_t retr_write_file(string_t *args, size_t length, string_t *data);

/**
  * sends a PWD command to the server in the current session
  * @param session - the session in which to send the PWD command
  */
status_t pwd_command(session_t *session);

/**
  * sends a HELP command to the server in the current session
  * @param session - the session in which to send the HELP command
  * @param args    - the command line argument array
  * @param length  - the length of the args array
  */
status_t help_command(session_t *session, string_t *args, size_t length);

/**
  * sends a QUIT command to the server in the current session to quit it
  * @param session - the session in which to send the PWD command
  */
status_t quit_command(session_t *session);

/**
  * sends a PORT command, either PORT or EPRT, depending on the settings within
  * session, returning the listening socket in listen_socket
  * @param session       - the session in which to send the PORT commnand
  * @param listen_socket - out param; is set to the socket to listen on
  * 	afterward the PORT command
  */
status_t port_command(session_t *session, int *listen_socket);

/**
  * sends a PASV command, either PASV or EPASV, depending on the settings within
  * session, returning the host and the port on the corresponding parameters
  * @param session - the session in which to send the PASV command
  * @param host    - out param; will contain the name of the host to which to
  *		connect
  * @param port    - out param; will contain the port to which to connect
  */
status_t pasv_command(session_t *session, string_t *host, uint16_t *port);

/**
  *	sets the passive flag in the session object; doesn't actually have to go to
  *	the server
  * @param session - the session object in which to set the flag
  */
status_t passive_command(session_t *session);

/**
  *	sets the extended flag in the session object; doesn't actually have to go to
  *	the server
  * @param session - the session object in which to set the flag
  */
status_t extended_command(session_t *session);

/**
  * helpter function for port_command; sets up a listening socket in the current
  * session, using internet protocol af and address address, setting
  * listen_socket and port appropriately when finished
  */
status_t set_up_listen_socket(session_t *session, int *listen_socket, uint16_t
	*listen_port, int af, char *address);

/**
  *
  */
status_t get_data_socket_active(session_t *session, int *data_socket,
	status_t (send_the_command)(session_t *, string_t *), string_t *args);

status_t get_data_socket_passive(session_t *session, int *data_socket, status_t
		(send_the_command)(session_t *, string_t *), string_t *args);
//---------------------------END COMMAND FUNCTIONS-----------------------------

//-----------------------------SOCKET FUNCTIONS--------------------------------
/**
  *	sends the command given by command in the current session
  * @param session - the session in which to send the command
  * @param command - the command to send
  */
status_t send_command(session_t *session, command_t *command);

/**
  *	reads an entire response from the server (i.e., until \r\n or another
  *	suitable end marker) into response
  * @param session  - the session in which the response will be read
  * @param response - out param; to where the response will be written. Must be
  * 	initialized
  */
status_t read_entire_response(session_t *session, string_t *response);

/**
  * In session, performs a send_command of command and then reads the response
  * into response using read_entire_response.
  * @param session  - the session in which to perform the actions
  * @param command  - the command to send
  * @param response - out param; to where the response will be written. Must be
  * 	initialized
  */
status_t send_command_read_response(session_t *session, command_t *command, string_t *response);

/**
  * reads from socket socket until a '\r\n' sequence is reached
  * @param socket - socket from which to read
  * @param line   - out param; the string into which to read. Must be initialized
  */
status_t read_single_line(int socket, string_t *line);

/**
  * after reading the first line, will continue reading other lines of a
  * multi-line response from the server
  * @param socket   - socket from which to read
  * @param response - out param; the string into which to read. Must be initialized
  */
status_t read_remaining_lines(int socket, string_t *response);

/**
  *	reads a single character from the socket
  * @param socket - socket from which to read
  * @param c      - pointer to character into which to read
  */
status_t read_single_character(int socket, char *c);

/**
  * reads from the given socket utnil EOF is reached (i.e., read returns 0)
  * @param socket - socket from which to read
  * @param response - out param; the string into which to read. Must be initialized
  */
status_t read_until_eof(int socket, string_t *response);
//----------------------------END SOCKET FUNCTIONS------------------------------

//-----------------------------HELPER FUNCTIONS--------------------------------
/**
  * Determines if the response matches the three digit FTP code in its first
  * three characters
  * @param response - the response to check for a match
  * @param code     - three digit FTP response code to match against
  * @return true of the response matches the code
  */
uint8_t matches_code(string_t *response, char *code);

/**
  * Performs a boolean memory compare of s1 and s2 over n bytes, returning true
  * of false as appropriate
  * @param s1 - the first memory array to compare
  * @param s2 - the second memory array to compare
  * @param n  - the length of array to check
  * @return true if the n first bytes of n1 and n2 ar the same
  */
uint8_t bool_memcmp(char *s1, char *s2, size_t n);

/**
  * Performs a strcmp and returns true if the result is 0, false otherwise
  * @param s1 - the first string to compare
  * @param s2 - the second string to compare
  * @return true if s1's bytes are equal to s2's bytes, up to the first null
  * 	terminator
  */
uint8_t bool_strcmp(char *s1, char *s2);

/**
  * logs the message of length into the log file given by session
  * @param session - the session in which the message will be written
  * @param message - the messsage to write to the log
  * @param length  - the length of the message
  */
status_t write_log(session_t *session, char *message, size_t length);

/**
  * write a "received" message to the log, with the received data
  * @param session - current session
  * @param message - the message received/to be written
  */
status_t write_received_message_to_log(session_t *session, string_t *message);

/**
  * write a "sent" message to the log, with the send data
  * @param session - current session
  * @param message - the message sent/to be written
  */
status_t write_sent_message_to_log(session_t *session, string_t *message);

/**
  * write a message to the log, prepended by "prepend" of length size
  * @param session - current session
  * @param message - the message to be written
  * @param prepend - some data to prepend to the message
  * @param size    - the length of prepend
  */
status_t prepend_and_write_to_log(session_t *session, string_t *message, char
	*prepend, size_t size);

/**
  * given an error code, prints a generic, stock error message for that type
  * @param error the error flag
  */
void print_error_message(status_t error);
//----------------------------END HELPER FUNCTIONS------------------------------

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
		printf("Could not get IP information.\n");
		goto exit2;
	}

	//try to default to non-passive mode (i.e., use PORT), but if have neither
	//ip address, need to use passive mode
	if (session.ip4 == NULL && session.ip6 == NULL)
	{
		session.passive_mode = 1;
		session.extended_mode = 0;
	}
	else
	{
		//One of ip4 or ip6 is not NULL, so active mode is possible
		session.passive_mode = 0;

		if (session.ip4 == NULL)
		{
			session.extended_mode = 1;
			printf("Could not find IPv4 address. Defaulting to extended mode.\n");
		}
		else
		{
			session.extended_mode = 0;
			if (session.ip6 == NULL)
			{
				printf("Could not find IPv6 address. Defaulting to non-extended mode.\n");
			}
		}
	}

	//begin the session
	error = do_session(&session);
	if (error)
	{
		goto exit3;
	}

	//error handling using gotos and exit labels and cleanup. At the end, return
	//the error from main for the shell to inspect
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
		//use a temporary integer because port is unsigned
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
	//Convert the port to network (i.e., big endian) byte order
	port = htons(port);
	struct hostent *host = gethostbyname(host_str);
	if (host == NULL)
	{
		return HOST_ERROR;
	}

	//Create an IPv4 TCP socket
	if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		return SOCKET_OPEN_ERROR;
	}

	//Create the connection
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
	//Create the file if it doesn't exist, and always append new logs to the end
	//of the file
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
			//try to set ip4 from ifa, ignoring the address if it's the loopback
			//address
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
			//try to set ip6 from ifa, ignoring the address if it's the loopback
			//address
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

	//Ignore localhosts
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
		
		//split the input along space lines, treating any consecutive spaces as
		//"one" space
		size_t array_length;
		string_t *args = string_split_skip_consecutive(&line, ' ',
			&array_length, 1);

		//Now determine the command type and execute it
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
		else if (bool_strcmp(c_str, "get"))
		{
			error = retr_command(session, args, array_length);
		}
		else if (bool_strcmp(c_str, "pwd"))
		{
			error = pwd_command(session);
		}
		else if (bool_strcmp(c_str, "help"))
		{
			error = help_command(session, args, array_length);
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
			error = NON_FATAL_ERROR;
		}

		print_error_message(error);

		size_t i = 0;
		for (i = 0; i < array_length; i++)
		{
			string_uninitialize(args + i);
		}
		free(args);

	} while (!quit && (!error || error == NON_FATAL_ERROR));

	if (error && error != NON_FATAL_ERROR)
	{
		printf("Fatal error. Exiting.\n");
	}

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

	//Read the entirety of the response
	error = read_entire_response(session, &response);
	if (error)
	{
		goto exit0;
	}

	//If the response is an intermediary one, then read another response
	if (matches_code(&response, SERVICE_READY_IN))
	{
		char_vector_clear(&response);
		error = read_entire_response(session, &response);
		if (error)
		{
			goto exit0;
		}
	}

	//If the service is not ready, then need to indicate this
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

	//If the response informs us that a password is required, then retrieve that
	//from the user and send it
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
		
		//If a password isn't needed, then continue on
		if (matches_code(&response, NOT_IMPLEMENTED_SUPERFLUOUS))
		{
			goto exit0;
		}
	}

	//If after submitting the password we're still not logged in, return an
	//error.
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

	//If the user is no longer logged in, return an error to the caller
	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit1;
	}

	//If the action was not successful, return an error to the caller, to let
	//them the call was not successful, but make it non-fatal, because execution
	//can continue
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

	//If the user is no longer logged in, return an error to the caller
	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit0;
	}

	//If the action was not successful, return an error to the caller, to let
	//them the call was not successful, but make it non-fatal, because execution
	//can continue
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
	char closing_message[] = "Closing data socket.\n";
	string_t *final_args = length > 1 ? args + 1 : NULL;
	
	//Depending whether the user is in passive or active mode, set up the data
	//socket for reading the LIST data, including sending the initial LIST
	//command and reading the response
	int data_socket; 
	if (!session->passive_mode)
	{
		error = get_data_socket_active(session, &data_socket,
			send_list_command, final_args);
	}
	else
	{
		error = get_data_socket_passive(session, &data_socket,
			send_list_command, final_args);
	}

	if (error)
	{
		goto exit0;
	}

	//read the data itself
	string_t data;
	string_initialize(&data);
	error = read_until_eof(data_socket, &data);
	if (error)
	{
		goto exit1;
	}
	//and print it
	printf("%s", string_c_str(&data));

	//read the second response from the server
	string_t response;
	string_initialize(&response);
	error = read_entire_response(session, &response);
	if (error)
	{
		goto exit2;
	}

	//If the action was not successful, return an error to the caller, to let
	//them the call was not successful, but make it non-fatal, because execution
	//can continue
	if (!matches_code(&response, CONNECTION_OPEN_NO_TRANSFER) &&
		!matches_code(&response, CLOSING_DATA_CONNECTION))
	{
		error = NON_FATAL_ERROR;
		goto exit2;
	}

exit2:
	string_uninitialize(&response);
exit1:
	string_uninitialize(&data);
	write_log(session, closing_message , sizeof closing_message - 1);
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

	//If not logged in, return an error to the caller to let them know.
	if (matches_code(&response, NOT_LOGGED_IN))
	{
		error = LOG_IN_ERROR;
		goto exit0;
	}

	//Otherwise, while not matching either of these means the command failed,
	//the error is non-fatal (i.e., the program can keep executing), so indicate
	//that
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

status_t retr_command(session_t *session, string_t *args, size_t length)
{
	char closing_message[] = "Closing data socket.\n";
	status_t error = SUCCESS;

	if (length <= 1)
	{
		printf("Please supply a file to get.\n");
		error = NON_FATAL_ERROR;
		goto exit0;
	}

	int data_socket; 
	if (!session->passive_mode)
	{
		error = get_data_socket_active(session, &data_socket, send_retr_command,
				args + 1);
	}
	else
	{
		error = get_data_socket_passive(session, &data_socket,
			send_retr_command, args + 1);
	}

	if (error)
	{
		goto exit0;
	}

	string_t data;
	string_initialize(&data);
	error = read_until_eof(data_socket, &data);
	if (error)
	{
		goto exit1;
	}

	string_t response;
	string_initialize(&response);
	error = read_entire_response(session, &response);
	if (error)
	{
		goto exit2;
	}

	if (!matches_code(&response, CONNECTION_OPEN_NO_TRANSFER) &&
		!matches_code(&response, CLOSING_DATA_CONNECTION))
	{
		error = NON_FATAL_ERROR;
		goto exit2;
	}

	error = retr_write_file(args, length, &data);
	if (error)
	{
		goto exit2;
	}

exit2:
	string_uninitialize(&response);
exit1:
	string_uninitialize(&data);
	error = write_log(session, closing_message , sizeof closing_message - 1);
	close(data_socket);
exit0:
	return error;
}

status_t send_retr_command(session_t *session, string_t *args)
{
	status_t error;
	string_t response;
	string_initialize(&response);

	MAKE_COMMAND_FROM_LITERAL(command, "RETR", args);

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

	if (!matches_code(&response, TRANSFER_STARTING) &&
		!matches_code(&response, FILE_STATUS_OKAY))
	{
		error = NON_FATAL_ERROR;
		goto exit1;
	}

exit1:
	string_uninitialize(&response);
exit0:
	return error;

}

status_t retr_write_file(string_t *args, size_t length, string_t *data)
{
	status_t error = SUCCESS;

	char *new_name;
	if (length > 2)
	{
		//if the user supplied a third argument, make that the name of the
		//output file
		new_name = string_c_str(args + 2);
	}
	else
	{
		//otherwise, default to the name of the server file
		new_name = string_c_str(args + 1);
	}

	int new_fd = open(new_name, O_CREAT | O_WRONLY, 0600);
	if (new_fd < 0)
	{
		error = FILE_OPEN_ERROR;
		goto exit0;
	}

	char *c_str = string_c_str(data);
	size_t data_size = string_length(data);
	size_t total_written = 0;
	while (total_written < data_size)
	{
		ssize_t written = write(new_fd, c_str + total_written, data_size - total_written);
		if (written < 0)
		{
			error = FILE_WRITE_ERROR;
			goto exit1;
		}

		total_written += data_size;
	}
exit1:
	close(new_fd);
exit0:
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
	//Split the response on the commas
	string_t *split = string_split(&response, ',', &len);
	//Then walk the first string until a '(' or a '=' is hit (which is returned
	//by the server is not defined in the protocol and is therefore
	//implementation dependent)
	while (char_vector_get(split + 0, 0) != '(' && char_vector_get(split + 0, 0) != '=')
	{
		char_vector_remove(split + 0, 0);
	}
	char_vector_remove(split + 0, 0);

	//Concatenate the first four array values into the IP address
	size_t i;
	for (i = 0; i < 4; i++)
	{
		string_concatenate(host, split + i);
		string_concatenate_char_array(host, ".");
		string_uninitialize(split + i);
	}
	char_vector_pop_back(host); //remove the last '.'

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

status_t help_command(session_t *session, string_t *args, size_t length)
{
	status_t error;

	//If the user provided other arguments, then append them all together to
	//send as part of the HELP command
	string_t *final_args;
	if (length > 1)
	{
		final_args = malloc(sizeof *final_args);
		if (final_args == NULL)
		{
			goto exit0;
		}

		string_initialize(final_args);
		size_t i;
		for (i = 0; i < length; i++);
		{
			string_concatenate(final_args, args + i);
			char_vector_push_back(final_args, ' ');
		}
		char_vector_pop_back(final_args);
	}
	else
	{
		//otherwise, don't pass any arguments
		final_args = NULL;
	}

	string_t response;
	string_initialize(&response);
	MAKE_COMMAND_FROM_LITERAL(command, "HELP", args);
	error = send_command_read_response(session, &command, &response);
	if (error)
	{
		goto exit1;
	}

	if (!matches_code(&response, SYSTEM_STATUS) &&
		!matches_code(&response, HELP_MESSAGE))
	{
		error = NON_FATAL_ERROR;
		goto exit1;
	}

exit2:
	string_uninitialize(&response);
exit1:
	free(final_args);
exit0:
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

	//QUIT can't return fatal error codes, and we're exiting anyway, so don't bother checking

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

	char *word;
	if (session->extended_mode)
	{
		if (session->ip4 == NULL)
		{
			printf("No IPv4 address was found, so extended mode cannot be turned off.\n");
			error = NON_FATAL_ERROR;
			goto exit0;
		}

		word = "off";
		session->extended_mode = 0;
	}
	else
	{
		if (session->ip4 == NULL)
		{
			printf("No IPv4 address was found, so extended mode cannot be turned off.\n");
			error = NON_FATAL_ERROR;
			goto exit0;
		}

		word = "on";
		session->extended_mode = 1;
	}

	printf("Extended mode is now %s.\n", word);

exit0:
	return error;
}

status_t set_up_listen_socket(session_t *session, int *listen_socket, uint16_t
	*listen_port, int af, char *address)
{
	status_t error = SUCCESS;

	//create a TCP socket using the given protocol
	*listen_socket = socket(af, SOCK_STREAM, 0);
	if (*listen_socket < 0)
	{
		error = SOCKET_OPEN_ERROR;
		goto exit_error0;
	}

	//use a sockaddr_in6 to supersed a sockaddr_in
	struct sockaddr_in6 sad;
	socklen_t size = sizeof sad;
	memset(&sad, 0, size);
	sad.sin6_family = af;
	inet_pton(af, address, &sad.sin6_addr);

	//bind the socket to a random available port (because sad.sin6_addr == 0)
	if (bind(*listen_socket, (struct sockaddr *) &sad, size) < 0)
	{
		error = BIND_ERROR;
		goto exit_error1;
	}

	//Make the socket listen
	if (listen(*listen_socket, 1) < 0)
	{
		error = LISTEN_ERROR;
		goto exit_error1;
	}

	//Get the socket name from the listen_sock to find out on which port it ended
	//up listening
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

status_t get_data_socket_active(session_t *session, int *data_socket,
	status_t (send_the_command)(session_t *, string_t *), string_t *args)
{
	status_t error;

	int listen_socket;
	error = port_command(session, &listen_socket);
	if (error)
	{
		goto exit0;
	}

	//send the first command (i.e., LIST) so that the server can know to start
	//trying to connect
	error = (*send_the_command)(session, args);//send_list_command(session, final_args);
	if (error)
	{
		goto exit1;
	}

	struct sockaddr_in6 cad;
	socklen_t cadlen = sizeof cad;
	*data_socket = accept(listen_socket, (struct sockaddr *) &cad, &cadlen);
	if (*data_socket < 0)
	{
		error = ACCEPT_ERROR;
		goto exit1;
	}

	char message[] = "Accepted connection on data socket.\n";
	error = write_log(session, message, sizeof message - 1);
	if (error)
	{
		close(data_socket);
		goto exit1;
	}

exit1:
	close(listen_socket);
exit0:
	return error;
}

status_t get_data_socket_passive(session_t *session, int *data_socket, status_t
		(send_the_command)(session_t *, string_t *), string_t *args)
{
	status_t error;

	string_t host;
	string_initialize(&host);
	uint16_t port;

	error = pasv_command(session, &host, &port);
	if (error)
	{
		goto exit_error0;
	}

	error = make_connection(data_socket, string_c_str(&host), port);
	if (error)
	{
		goto exit_error0;
	}

	char message[] = "Made connection to server for data socket.\n";
	error = write_log(session, message, sizeof message - 1);
	if (error)
	{
		goto exit_error1;
	}

	//Send the initial command so that the server will start listening on the
	//socket
	error = (*send_the_command)(session, args);
	if (error)
	{
		goto exit_error1;
	}

	//equivalent to return SUCCESS; (skips closing the data_socket);
	goto exit_success0;

exit_error1:
	close(*data_socket);

exit_error0:
exit_success0:
	string_uninitialize(&host);
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

	error = write_sent_message_to_log(session, &command_string);
	if (error)
	{
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
		//if the fourth character (i.e. index 3) is a '-', then the response is
		//multiline. Multiline responses are delineated in a special way, so
		//pass them off here to read the rest of it
		error = read_remaining_lines(session->command_socket, response);
		if (error)
		{
			goto exit0;
		}
	}

	char *c_str = string_c_str(response);
	printf("%s", c_str);
	
	error = write_received_message_to_log(session, response);
	if (error)
	{
		goto exit0;
	}

	//This error is common across nearly every single command type, so treat it
	//uniformly here, and treat it like a fatal error
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
	
	//Read one character first so that the check that the last two characters
	//are '\r' and '\n' below is valid
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
	//keep reading until a '\r' and then a '\n' are found

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

	//use a bigger buffer rather than reading single character at a time
	//because:
	//	A.) Not looking for specific character sequence to end at
	//	B.) For efficiency's sake - data port might transfer much more data
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

status_t write_log(session_t *session, char *message, size_t length)
{
	status_t error = SUCCESS;

	time_t time_val = time(NULL);
	if (time_val < 0)
	{
		error = TIME_GET_ERROR;
		goto exit0;
	}

	char *time_val_string = ctime(&time_val);
	if (time_val_string == NULL)
	{
		error = TIME_STRING_ERROR;
		goto exit0;
	}
	size_t time_len = strlen(time_val_string);
	time_val_string[time_len - 1] = ' ';

	if (write(session->log_file, time_val_string, time_len) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit0;
	}

	if (write(session->log_file, message, length) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit0;
	}

	if (write(session->log_file, "\n", 1) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit0;
	}

exit0:
	return error;
}

status_t write_received_message_to_log(session_t *session, string_t *message)
{
	char received[] = "Received: ";
	return prepend_and_write_to_log(session, message, received,
		sizeof received - 1);
}

status_t write_sent_message_to_log(session_t *session, string_t *message)
{
	char received[] = "Sent: ";
	return prepend_and_write_to_log(session, message, received,
		sizeof received - 1);
}

status_t prepend_and_write_to_log(session_t *session, string_t *message, char
	*prepend, size_t size)
{
	status_t error = SUCCESS;

	string_t final_message;
	string_initialize(&final_message);
	string_assign_from_char_array_with_size(&final_message, prepend, size);
	string_concatenate(&final_message, message);

	error = write_log(session, string_c_str(&final_message),
		string_length(&final_message));
	if (error)
	{
		goto exit0;
	}

exit0:
	string_uninitialize(&final_message);
	return error;

}

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
		default:
			printf("Unknown error: %lu\n", error);
			break;
	}
}
