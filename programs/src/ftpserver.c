#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "accounts.h"
#include "ftp.h"
#include "log.h"
#include "server.h"
#include "status_t.h"
#include "string_t.h"

#define ARGC 2

#define MAX_USERS 30

#define HELP_STRING "CDUP CWD EPRT EPSV\r\n"\
	"HELP LIST PASS PASV\r\n"\
	"PORT PWD QUIT RETR USER"

/**
  * Structure for holding all the information a user thread needs for processing
  * command_sock - the socket over which the commands are sent
  * server - reference to the server configuration object
  * account - the user's account structure
  * logged_in - flag indicating whether user has successfully logged in
  * directory - the representation of the user's current working directory
  * data_sock - the socket over which data will be sent
  */
typedef struct
{
	int command_sock;
	server_t *server;
	account_t *account;
	uint8_t logged_in;
	char *directory;
	int data_sock;
} user_session_t;

/**
  * Parses the command line, returning an error if there are any problems with
  * it, and otherwise placing the passed port number into *port
  * @param argc - number of arguments to main
  * @param argv - the arguments to main themselves
  * @param port - out param; holds the port number (argument 1/argv[1])
  */
status_t parse_command_line(int argc, char *argv[], uint16_t *port);

/**
  * Given a response code and a message to include with it, this function will
  * stitch them up into the correct format to send over the socket sock. The
  * sending will be recorded in the log file indicated by log, and a multiline
  * is a flag with an obvious purpose
  * @param sock - the socket over which to send the data
  * @param code - the code to send in the response
  * @param message - the message to include with the response
  * @param log - the log file to which to log the sending
  * @param
  */
status_t send_response(int sock, char *code, char *message, log_t *log, uint8_t multiline);

/**
  * Sends pure data in the string over the socket, without adding line endings
  * or anything else
  * @param sock - the socket over which to send the data
  * @param s - the string to send
  * @param log - the log in which to log that data was sent
  */
status_t send_data_string(int sock, string_t *s, log_t *log);

/**
  * The "thread" function for each client/user that connects. Continuously loops
  * until an error is encountered or until the user enters the "quit" command
  * @param void_args - pointer to the thread argument structure. Actually of
  * 	type user_session_t *
  */
void *client_handler(void *void_args);

/**
  * These functions all serve the purpose of handling the commands from the user
  * once they have been read and parsed.
  * @param session - the current session for the user
  * @param args - an array of arguments that arrived with the user's command
  * @param len - the length of the args array
  */
status_t handle_user_command(user_session_t *session, string_t *args, size_t len);
status_t handle_pass_command(user_session_t *session, string_t *args, size_t len);
status_t handle_cwd_command(user_session_t *session, string_t *args, size_t len);
status_t handle_cdup_command(user_session_t *session, string_t *args, size_t len);
status_t handle_quit_command(user_session_t *session, string_t *args, size_t len);
status_t handle_pasv_command(user_session_t *session, string_t *args, size_t len);
status_t handle_epsv_command(user_session_t *session, string_t *args, size_t len);
status_t handle_port_command(user_session_t *session, string_t *args, size_t len);
status_t handle_eprt_command(user_session_t *session, string_t *args, size_t len);
status_t handle_retr_command(user_session_t *session, string_t *args, size_t len);
status_t handle_pwd_command(user_session_t *session, string_t *args, size_t len);
status_t handle_list_command(user_session_t *session, string_t *args, size_t len);
status_t handle_help_command(user_session_t *session, string_t *args, size_t len);
status_t handle_unrecognized_command(user_session_t *session, string_t *args, size_t len);

/**
  * The following functions are all rather straightforward - in the current
  * session for the user, they send a static (in all but one case) message back
  * to the client. The one exception is send_257, which uses the directory
  * information in session to send the current working directory to the client.
  */
status_t send_125(user_session_t *session);
status_t send_200(user_session_t *session);
status_t send_214(user_session_t *session);
status_t send_221(user_session_t *session);
status_t send_226(user_session_t *session);
status_t send_250(user_session_t *session);
status_t send_257(user_session_t *session);
status_t send_330(user_session_t *session);
status_t send_331(user_session_t *session);
status_t send_425(user_session_t *session);
status_t send_451(user_session_t *session);
status_t send_500(user_session_t *session);
status_t send_501(user_session_t *session);
status_t send_502(user_session_t *session);
status_t send_503(user_session_t *session);
status_t send_530(user_session_t *session);
status_t send_550(user_session_t *session);

/**
  * Determines whether a particular path is a directory or not
  * @param dir - the path to check
  */
uint8_t is_directory(char *dir);

int main(int argc, char *argv[])
{
	status_t error;

	server_t server;
	error = initialize_server(&server);
	if (error)
	{
		goto exit0;
	}

	char start_up_message[] = "Config file read. Starting rest of server up.\n";
	error = write_log(server.log, start_up_message, sizeof start_up_message);
	if (error)
	{
		goto exit0;
	}

	uint16_t port;
	error = parse_command_line(argc, argv, &port);
	if (error)
	{
		goto exit0;
	}

	char socket_message[] = "Setting up socket.\n";
	error = write_log(server.log, socket_message, sizeof socket_message);
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
			char *error_str = get_error_message(ACCEPT_ERROR);
			write_log(server.log, error_str, strlen(error_str));
			printf("%s", error_str);
		}
		else
		{
			char join_message[] = "Client joined.\n";
			write_log(server.log, join_message, sizeof join_message);
			printf("%s", join_message);

			//use calloc to make sure the state flags are all set to 0.
			user_session_t *args = calloc(1, sizeof *args);
			args->command_sock = connection_sock;
			args->server = &server;

			pthread_t thread;
			if (pthread_create(&thread, NULL, client_handler, args) != 0)
			{
				error = send_response(connection_sock, SERVICE_NOT_AVAILABLE, "Could not establish a session.", server.log, 0);

				char *error_str = get_error_message(PTHREAD_CREATE_ERROR);
				write_log(server.log, error_str, strlen(error_str));
				printf("%s", error_str);
			}
			else
				pthread_detach(thread);
		}
	}

	char closing_message[] = "Server closing down.\n";
exit1:
	write_log(server.log, closing_message, sizeof closing_message);
	close(listen_sock);
exit0:
	free_server(&server);
	print_error_message(error);
	return error;
}

status_t parse_command_line(int argc, char *argv[], uint16_t *port)
{
	if (argc != ARGC)
	{
		printf("Usage: ftpserver port\n");
		return BAD_COMMAND_LINE;
	}

	int tmp = atoi(argv[1]);
	if (tmp <= 0 || tmp > UINT16_MAX)
	{
		printf("Port number must be positive and less than or equal to %u.\n", UINT16_MAX);
		return BAD_COMMAND_LINE;
	}
	*port = tmp;

	return SUCCESS;
}

void *client_handler(void *void_args)
{
	user_session_t *session = (user_session_t *) void_args;
	status_t error;

	//Finish session initialization
	session->directory = realpath(".", NULL);
	if (session->directory == NULL)
	{
		error = REALPATH_ERROR;
		goto exit0;
	}
	session->data_sock = -1;
	//End session initialization

	error = send_response(session->command_sock, SERVICE_READY, "Ready. Please send USER.", session->server->log, 0);
	if (error)
	{
		goto exit1;
	}

	string_t command;
	string_initialize(&command);

	uint8_t done = 0;
	do
	{
		char_vector_clear(&command);
		error = read_single_line(session->command_sock, &command);
		if (!error)
		{
			error = write_received_message_to_log(session->server->log, &command);
			if (!error)
			{
				//Remove the CRLF from the command
				char_vector_pop_back(&command);
				char_vector_pop_back(&command);

				//Split the string up by spaces
				size_t len;
				string_t *split = string_split_skip_consecutive(&command, ' ', &len, 1);
				char *c_str = string_c_str(split + 0);

				//Determine which command has been sent
				if (bool_strcmp(c_str, "USER"))
				{
					error = handle_user_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "PASS"))
				{
					error = handle_pass_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "CWD"))
				{
					error = handle_cwd_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "CDUP"))
				{
					error = handle_cdup_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "QUIT"))
				{
					done = 1;
					error = handle_quit_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "PASV"))
				{
					error = handle_pasv_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "EPSV"))
				{
					error = handle_epsv_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "PORT"))
				{
					error = handle_port_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "EPRT"))
				{
					error = handle_eprt_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "RETR"))
				{
					error = handle_retr_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "PWD"))
				{
					error = handle_pwd_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "LIST"))
				{
					error = handle_list_command(session, split, len);
				}
				else if (bool_strcmp(c_str, "HELP"))
				{
					error = handle_help_command(session, split, len);
				}
				else
				{
					error = handle_unrecognized_command(session, split, len);
				}

				size_t i;
				for (i = 0; i < len; i++)
				{
					string_uninitialize(split + i);
				}
				free(split);
			}
		}

		if (error)
		{
			char message[] = "Error encountered while processing: ";
			string_t error_message;
			string_initialize(&error_message);
			char *error_string = get_error_message(error);
			string_assign_from_char_array(&error_message, error_string);
			prepend_and_write_to_log(session->server->log, &error_message, message, sizeof message);
			string_uninitialize(&error_message);
		}
	} while (!error && !done);

	char quitting_message[] = "Client quitting.\n";

exit2:
	string_uninitialize(&command);
exit1:
	free(session->directory);
exit0:
	write_log(session->server->log, quitting_message, sizeof quitting_message);
	printf("%s", quitting_message);
	free(session);
	pthread_exit(NULL);
}

status_t handle_user_command(user_session_t *session, string_t *args, size_t len)
{
	status_t error;

	if (session->logged_in)
	{
		error = send_330(session);
		goto exit0;
	}

	if (len < 2)
	{
		error = send_501(session);
		goto exit0;
	}

	string_t username;
	string_initialize(&username);
	size_t i;
	for (i = 1; i < len; i++)
	{
		string_concatenate(&username, args + i);
	}

	get_account_by_username(session->server->accounts, string_c_str(&username), &session->account);
	if (session->account == NULL)
	{
		error = send_530(session);
		goto exit1;
	}

	error = send_331(session);
	if (error)
	{
		goto exit1;
	}

exit1:
	string_uninitialize(&username);
exit0:
	return error;
}

status_t handle_pass_command(user_session_t *session, string_t *args, size_t len)
{
	status_t error;

	//Make sure that a USER command has already been executed by checking this
	//pointer value
	if (session->account == NULL)
	{
		error = send_503(session);
		goto exit0;
	}

	if (len < 2)
	{
		error = send_501(session);
		goto exit0;
	}

	string_t password;
	string_initialize(&password);

	size_t i;
	for (i = 1; i < len; i++)
	{
		string_concatenate(&password, args + i);
	}

	if (!bool_strcmp(string_c_str(&password), session->account->password))
	{
		error = send_530(session);
	}

	error = send_330(session);
	if (error)
	{
		goto exit1;
	}

	session->logged_in = 1;

exit1:
	string_uninitialize(&password);
exit0:
	return error;
}

status_t handle_cwd_command(user_session_t *session, string_t *args, size_t len)
{
	status_t error;

	if (!session->logged_in)
	{
		error = send_530(session);
		goto exit0;
	}

	if (len < 2)
	{
		error = send_501(session);
		goto exit0;
	}

	string_t new_dir;
	string_initialize(&new_dir);

	char *first_c_str = string_c_str(args + 1);
	if (first_c_str[0] != '/' && first_c_str[0] != '~')
	{
		string_assign_from_char_array(&new_dir, session->directory);
		char_vector_push_back(&new_dir, '/');
	}

	size_t i;
	for (i = 1; i < len; i++)
	{
		string_concatenate(&new_dir, args + i);
	}

	char *resolved_dir = realpath(string_c_str(&new_dir), NULL);
	if (resolved_dir == NULL || !is_directory(resolved_dir))
	{
		error = send_550(session);
		goto exit1;
	}

	free(session->directory);
	session->directory = resolved_dir;

	error = send_250(session);
	if (error)
	{
		goto exit1;
	}

exit1:
	string_uninitialize(&new_dir);
exit0:
	return error;
}

status_t handle_cdup_command(user_session_t *session, string_t *args, size_t len)
{
	status_t error;
	if (!session->logged_in)
	{
		error = send_530(session);
		goto exit0;
	}

	string_t s;
	string_initialize(&s);
	string_assign_from_char_array(&s, session->directory);
	string_concatenate_char_array_with_size(&s, "/..", 3);

	char *resolved_dir = realpath(string_c_str(&s), NULL);
	if (resolved_dir == NULL || !is_directory(resolved_dir))
	{
		error = send_550(session);
		goto exit1;
	}

	free(session->directory);
	session->directory = resolved_dir;

	error = send_200(session);
	if (error)
	{
		goto exit1;
	}

exit1:
	string_uninitialize(&s);
exit0:
	return error;
}

status_t handle_quit_command(user_session_t *session, string_t *args, size_t len)
{
	session->logged_in = 0;
	return send_221(session);
}

status_t handle_pasv_command(user_session_t *session, string_t *args, size_t len)
{
	status_t error = SUCCESS;
	if (!session->server->pasv_enabled)
	{
		error = send_502(session);
		goto exit0;
	}

	if (!session->logged_in)
	{
		error = send_530(session);
		goto exit0;
	}

	int listen_sock;
	uint16_t listen_port;
	error = set_up_listen_socket(&listen_sock, &listen_port, AF_INET, session->server->ip4);
	if (error)
	{
		goto exit0;
	}

	string_t message;
	string_initialize(&message);
	string_assign_from_char_array(&message, "Entering passive mode (");
	create_comma_delimited_address(&message, session->server->ip4, listen_port);
	char_vector_push_back(&message, ')');

	error = send_response(session->command_sock, ENTERING_PASSIVE_MODE, string_c_str(&message), session->server->log, 0);
	if (error)
	{
		goto exit2;
	}

	struct sockaddr_in cad;
	socklen_t clilen = sizeof cad;
	session->data_sock = accept(listen_sock, (struct sockaddr *) &cad, &clilen);
	if (session->data_sock < 0)
	{
		error = ACCEPT_ERROR;
		goto exit2;
	}

exit2:
	close(listen_sock);
exit1:
	string_uninitialize(&message);
exit0:
	return error;
}

status_t handle_epsv_command(user_session_t *session, string_t *args, size_t len)
{
	return handle_unrecognized_command(session, args, len);
}

status_t handle_port_command(user_session_t *session, string_t *args, size_t len)
{
	status_t error;
	if (!session->server->port_enabled)
	{
		//This command can't actually send back a 'not implemented' response, so
		//approximate it with an "unrecognized command" response
		error = send_500(session);
		goto exit0;
	}

	if (!session->logged_in)
	{
		error = send_530(session);
		goto exit0;
	}

	size_t ip_len;
	string_t *split = string_split(args + 1, ',', &ip_len);

	string_t host;
	string_initialize(&host);
	uint16_t port;
	error = parse_ip_and_port(split, ip_len, &host, &port);
	if (error)
	{
		error = send_501(session);
		goto exit1;
	}

	error = make_connection(&session->data_sock, string_c_str(&host), port);
	if (error)
	{
		send_response(session->command_sock, SERVICE_NOT_AVAILABLE, "Could not connect to port", session->server->log, 0);
		goto exit1;
	}

	error = send_200(session);
	if (error)
	{
		goto exit1;
	}

exit1:
	string_uninitialize(&host);
exit0:
	return error;
}

status_t handle_eprt_command(user_session_t *session, string_t *args, size_t len)
{
	return handle_unrecognized_command(session, args, len);
}

status_t handle_retr_command(user_session_t *session, string_t *args, size_t len)
{
	/**
	  * Possible codes:
	  *		125: transfer_starting
	  *		150: about to open data connection
	  *		226: Completed successfully; closing
	  *		250: Requested file action completed
	  *		425: Can't open data connection
	  *		426: Connection closed
	  *		451: Action aborted; local error
	  *		450: File unavailable (busy)
	  *		550: File unavailable (doesn't exit)
	  *		500, 501, 421
	  */
	status_t error;
	if (!session->logged_in)
	{
		error = send_530(session);
		goto exit0;
	}

	if (session->data_sock < 0)
	{
		error = send_425(session);
		goto exit0;
	}

	if (len < 2)
	{
		error = send_501(session);
		goto exit1;
	}

	string_t path;
	string_initialize(&path);
	string_assign_from_char_array(&path, session->directory);
	char_vector_push_back(&path, '/');
	string_concatenate(&path, args + 1);

	int fd = open(string_c_str(&path), O_RDONLY, 0);
	if (fd < 0)
	{
		error = send_550(session);
		goto exit2;
	}

	string_t file_string;
	string_initialize(&file_string);
	ssize_t chars_read;
	char buff[512];
	while ((chars_read = read(fd, buff, sizeof buff)) > 0)
	{
		string_concatenate_char_array_with_size(&file_string, buff, sizeof buff);
	}

	if (chars_read < 0)
	{
		error = send_451(session);
		goto exit3;
	}

	error = send_125(session);
	if (error)
	{
		//let the first error supercede any that might occur here,
		//so don't save to error
		send_451(session);
		goto exit3;
	}

	error = send_data_string(session->data_sock, &file_string, session->server->log);
	if (error)
	{
		send_451(session);
	}
	else
	{
		error = send_226(session);
	}

exit3:
	string_uninitialize(&file_string);
exit2:
	string_uninitialize(&path);
exit1:
	close(session->data_sock);
	session->data_sock = -1;
exit0:
	return error;
}

status_t handle_pwd_command(user_session_t *session, string_t *args, size_t len)
{
	//This command cannot return "not logged in" error messages, and the only
	//other errors are "syntax errors." Ignore any possible "syntax errors" in
	//the args > 0 and just send the PWD
	return send_257(session);
}

status_t handle_list_command(user_session_t *session, string_t *args, size_t len)
{
	/*
		Possible codes:
			125: Data connection already open; transfer starting
			150: File status okay; about to open data connection
				226: Closing data connection; success
				250: File action okay, completed
				425: Can't open data connection
				426: Connection closed
				451: Aborted; local error
			450: Requested file action not taken
			500: Syntax error; command unrecognized
			501: Syntax error in params
			502: Not implemented
			421: Service connection closing
			530: Not logged in
	*/

	status_t error;

	if (!session->logged_in)
	{
		error = send_530(session);
		goto exit0;
	}

	if (session->data_sock < 0)
	{
		error = send_425(session);
		goto exit0;
	}

	string_t listing;
	string_initialize(&listing);

	string_t tmp;
	string_initialize(&tmp);

	DIR *directory;


	if (len < 2)
	{
		directory = opendir(session->directory);
		if (directory == NULL)
		{
			error = send_451(session);
			goto exit1;
		}

		struct dirent *entry;
		while ((entry = readdir(directory)))
		{
			string_concatenate_char_array(&listing, entry->d_name);
			char_vector_push_back(&listing, '\n');
		}

		closedir(directory);
	}
	else
	{
		string_assign_from_char_array(&tmp, session->directory);
		char_vector_push_back(&tmp, '/');
		string_concatenate(&tmp, args + 1);
		if (access(string_c_str(&tmp), F_OK) < 0)
		{
			error = send_501(session);
			goto exit1;
		}

		directory = opendir(string_c_str(&tmp));
		if (directory == NULL)
		{
			if (errno != ENOTDIR)
			{
				error = send_451(session);
				goto exit1;
			}

			//Already know that the file exists because of the call to access,
			//so if the file is not a directory, assuming that it's a regular
			//file, so just list it. This could also be checked using the stat
			//function.
			string_concatenate(&listing, args + 1);
			char_vector_push_back(&listing, '\n');
		}
		else
		{
			struct dirent *entry;
			while ((entry = readdir(directory)))
			{
				string_concatenate_char_array(&listing, entry->d_name);
				char_vector_push_back(&listing, '\n');
			}

			closedir(directory);
		}
	}

	error = send_125(session);
	if (error)
	{
		send_451(session);
		goto exit1;
	}

	error = send_data_string(session->data_sock, &listing, session->server->log);
	if (error)
	{
		send_451(session);
	}
	else
	{
		error = send_226(session);
	}

exit1:
	string_uninitialize(&tmp);
	string_uninitialize(&listing);
	close(session->data_sock);
	session->data_sock = -1;
exit0:
	return error;
}

status_t handle_help_command(user_session_t *session, string_t *args, size_t len)
{
	//This command cannot return "not logged in" error messages, and the only
	//other errors are "syntax errors." Ignore any possible "syntax errors" in
	//the args > 0 and just send the HELP
	return send_214(session);
}

status_t send_response(int sock, char *code, char *message, log_t *log, uint8_t multiline)
{
	status_t error;

	char sep;
	if (multiline)
	{
		sep = '-';
	}
	else
	{
		sep = ' ';
	}

	string_t response;
	string_initialize(&response);
	string_assign_from_char_array_with_size(&response, code, 3);
	char_vector_push_back(&response, sep);
	string_concatenate_char_array(&response, message);
	string_concatenate_char_array(&response, "\r\n");

	if (multiline)
	{
		string_concatenate_char_array_with_size(&response, code, 3);
		string_concatenate_char_array_with_size(&response, " \r\n", 3);
	}

	error = send_string(sock, &response, log);
	if (error)
	{
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	return error;
}

status_t send_data_string(int sock, string_t *s, log_t *log)
{
	status_t error = SUCCESS;
	char sending_data[] = "Sending data.\n";
	write_log(log, sending_data, sizeof sending_data);

	if (write(sock, string_c_str(s), string_length(s)) < 0)
	{
		char error_sending[] = "Error sending data.\n";
		write_log(log, error_sending, sizeof error_sending);
		error = SOCKET_WRITE_ERROR;
		goto exit0;
	}

	char data_sent[] = "Data sent.\n";
	write_log(log, data_sent, sizeof data_sent);

exit0:
	return error;
}

status_t handle_unrecognized_command(user_session_t *session, string_t *args, size_t len)
{
	return send_502(session);
}

status_t send_125(user_session_t *session)
{
	return send_response(session->command_sock, TRANSFER_STARTING, "Connection open. Transfer starting.", session->server->log, 0);
}

status_t send_200(user_session_t *session)
{
	return send_response(session->command_sock, COMMAND_OKAY, "Command okay.", session->server->log, 0);
}

status_t send_214(user_session_t *session)
{
	return send_response(session->command_sock, HELP_MESSAGE, HELP_STRING, session->server->log, 1);
}

status_t send_221(user_session_t *session)
{
	return send_response(session->command_sock, CLOSING_CONNECTION, "Goodbye.", session->server->log, 0);
}

status_t send_226(user_session_t *session)
{
	return send_response(session->command_sock, CLOSING_DATA_CONNECTION, "Data transfer succesful. Closing connection.", session->server->log, 0);
}

status_t send_250(user_session_t *session)
{
	return send_response(session->command_sock, FILE_ACTION_COMPLETED, "Action successful.", session->server->log, 0);
}

status_t send_257(user_session_t *session)
{
	string_t wd;
	string_initialize(&wd);
	string_assign_from_char_array(&wd, "\"");
	string_concatenate_char_array(&wd, session->directory);
	string_concatenate_char_array(&wd, "\"");

	status_t error = send_response(session->command_sock, PATH_CREATED, string_c_str(&wd), session->server->log, 0);

	string_uninitialize(&wd);
	return error;
}

status_t send_330(user_session_t *session)
{
	return send_response(session->command_sock, USER_LOGGED_IN, "Logged in.", session->server->log, 0);
}

status_t send_331(user_session_t *session)
{
	return send_response(session->command_sock, NEED_PASSWORD, "Username good. Please send password.", session->server->log, 0);
}

status_t send_425(user_session_t *session)
{
	return send_response(session->command_sock, CANT_OPEN_DATA_CONNECTION, "Data connection not open.", session->server->log, 0);
}

status_t send_451(user_session_t *session)
{
	return send_response(session->command_sock, ACTION_ABORTED_LOCAL_ERROR, "Local error. Aborting.", session->server->log, 0);
}

status_t send_500(user_session_t *session)
{
	return send_response(session->command_sock, COMMAND_UNRECOGNIZED, "Unrecognized command.", session->server->log, 0);
}

status_t send_501(user_session_t *session)
{
	return send_response(session->command_sock, SYNTAX_ERROR, "Error in command parameters.", session->server->log, 0);
}

status_t send_502(user_session_t *session)
{
	return send_response(session->command_sock, NOT_IMPLEMENTED, "Given command not implemented.", session->server->log, 0);
}

status_t send_503(user_session_t *session)
{
	return send_response(session->command_sock, BAD_SEQUENCE, "Please check the command sequence.", session->server->log, 0);
}

status_t send_530(user_session_t *session)
{
	return send_response(session->command_sock, NOT_LOGGED_IN, "Not logged in.", session->server->log, 0);
}

status_t send_550(user_session_t *session)
{
	return send_response(session->command_sock, ACTION_NOT_TAKEN_FILE_UNAVAILABLE2, "Requested action not completed.", session->server->log, 0);
}

uint8_t is_directory(char *dir)
{
	struct stat dirstat;
	if (stat(dir, &dirstat) != 0)
	{
		return 0;
	}

	return S_ISDIR(dirstat.st_mode);
}
