#include <arpa/inet.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include "ftp.h"
#include "log.h"
#include "status_t.h"
#include "string_t.h"

#define MINIMUM_ARGC 3
#define DEFAULT_COMMAND_PORT 21

#define MAKE_COMMAND_FROM_LITERAL(var, command, str_args)\
	command_t var;\
	var.identifier = command;\
	var.ident_n = sizeof command;\
	var.args = str_args

typedef struct
{
	int command_socket;
	log_t log;
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
  * after reading the first line, will continue reading other lines of a
  * multi-line response from the server
  * @param socket   - socket from which to read
  * @param response - out param; the string into which to read. Must be initialized
  */
status_t read_remaining_lines(int socket, string_t *response);

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

	error = open_log_file(&session.log, argv[2], 0);
	if (error)
	{
		goto exit1;
	}

	error = get_ips(&session.ip4, &session.ip6);
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
	close_log_file(&session.log);
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
	write_log(&session->log, closing_message , sizeof closing_message - 1);
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
	error = write_log(&session->log, closing_message , sizeof closing_message - 1);
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
	error = set_up_listen_socket(listen_socket, &listen_port, af, address);
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
		create_comma_delimited_address(&args, session->ip4, listen_port);

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

	error = parse_ip_and_port(split, len, host, port);
	if (error)
	{
		goto exit0;
	}

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
		for (i = 1; i < length; i++)
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
	if (final_args != NULL)
	{
		string_uninitialize(final_args);
		free(final_args);
	}
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
	error = write_log(&session->log, message, sizeof message - 1);
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
	error = write_log(&session->log, message, sizeof message - 1);
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

	error = send_string(session->command_socket, &command_string, &session->log);
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
	
	error = write_received_message_to_log(&session->log, response);
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
