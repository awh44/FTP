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
#include <sys/stat.h>
#include <time.h>

#include "ftp.h"
#include "log.h"
#include "status_t.h"
#include "string_t.h"

#define ARGC 3

#define MAX_USERS 30

typedef struct
{
	int log_file;
	pthread_mutex_t lock;
} log_t;

typedef struct account
{
	char *username;
	char *password;
	struct account *next;
} account_t;

#define ACCOUNT_BUCKETS 512
typedef struct
{
	account_t *accounts[ACCOUNT_BUCKETS];
} accounts_table_t;

typedef struct
{
	int command_sock;
	log_t *log;
	accounts_table_t *accounts;
	account_t *account;
	uint8_t logged_in;
	char *directory;
} user_session_t;

status_t parse_command_line(int argc, char *argv[], uint16_t *port);
status_t get_accounts(accounts_table_t *accounts);
status_t free_accounts(accounts_table_t *accounts);

status_t send_response(int sock, char *code, char *message, log_t *log);

void *client_handler(void *void_args);
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

uint8_t bool_strcmp(char *s1, char *s2);
uint8_t is_directory(char *dir);

status_t send_200(user_session_t *session)
{
	return send_response(session->command_sock, COMMAND_OKAY, "Command okay.", session->log);
}

status_t send_221(user_session_t *session)
{
	return send_response(session->command_sock, CLOSING_CONNECTION, "Goodbye.", session->log);
}

status_t send_250(user_session_t *session)
{
	return send_response(session->command_sock, FILE_ACTION_COMPLETED, "Action successful.", session->log);
}

status_t send_257(user_session_t *session)
{
	string_t wd;
	string_initialize(&wd);
	string_assign_from_char_array(&wd, "\"");
	string_concatenate_char_array(&wd, session->directory);
	string_concatenate_char_array(&wd, "\"");

	status_t error = send_response(session->command_sock, PATH_CREATED,
			string_c_str(&wd), session->log);

	string_uninitialize(&wd);
	return error;
}

status_t send_330(user_session_t *session)
{
	return send_response(session->command_sock, USER_LOGGED_IN, "Logged in.", session->log);
}

status_t send_331(user_session_t *session)
{
	return send_response(session->command_sock, NEED_PASSWORD, "Username good. Please send password.", session->log);
}

status_t send_501(user_session_t *session)
{
	return send_response(session->command_sock, SYNTAX_ERROR, "Error in command parameters.", session->log);
}

status_t send_503(user_session_t *session)
{
	return send_response(session->command_sock, BAD_SEQUENCE, "Please check the command sequence.", session->log);
}

status_t send_530(user_session_t *session)
{
	return send_response(session->command_sock, NOT_LOGGED_IN, "Not logged in.", session->log);
}

status_t send_550(user_session_t *session)
{
	return send_response(session->command_sock, ACTION_NOT_TAKEN_FILE_UNAVAILABLE2, "Requested action not completed.", session->log);
}

int main(int argc, char *argv[])
{
	status_t error;
	
	uint16_t port;
	error = parse_command_line(argc, argv, &port);
	if (error)
	{
		goto exit0;
	}

	log_t log;
	error = open_log_file(&log.log_file, argv[1]);
	if (error)
	{
		goto exit0;
	}

	accounts_table_t accounts;
	error = get_accounts(&accounts);
	if (error)
	{
		goto exit1;
	}

	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
	{
		error = SOCKET_OPEN_ERROR;
		goto exit2;
	}

	struct sockaddr_in sad;
	memset(&sad, 0, sizeof sad);
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = INADDR_ANY;
	sad.sin_port = htons(port);

	if (bind(listen_sock, (struct sockaddr *) &sad, sizeof sad) < 0)
	{
		error = BIND_ERROR;
		goto exit3;
	}

	if (listen(listen_sock, MAX_USERS) < 0)
	{
		error = LISTEN_ERROR;
		goto exit3;
	}

	struct sockaddr_in cad;
	socklen_t clilen = sizeof cad;

	while (1)
	{
		int connection_sock = accept(listen_sock, (struct sockaddr *) &cad, &clilen);
		if (connection_sock < 0)
		{
			print_error_message(ACCEPT_ERROR);
		}
		else
		{
			//use calloc to make sure the state flags are all set to 0.
			user_session_t *args = calloc(1, sizeof *args);
			args->command_sock = connection_sock;
			args->log = &log;
			args->accounts = &accounts;

			pthread_t thread;
			if (pthread_create(&thread, NULL, client_handler, args) != 0)
			{
				print_error_message(PTHREAD_CREATE_ERROR);
				
				error = send_response(connection_sock, SERVICE_NOT_AVAILABLE,
					"Could not establish a session.", &log);
				print_error_message(error);
			}
		}
	}

exit3:
	close(listen_sock);
exit2:
	free_accounts(&accounts);
exit1:
	close(log.log_file);
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

size_t hash(char *str)
{
    size_t hash_val = 5381;
    char c;

    while ((c = *str++))
    {
        hash_val = ((hash_val << 5) + hash_val) + c;
    }

    return hash_val;	
}

size_t accounts_hash(char *str)
{
	return hash(str) % ACCOUNT_BUCKETS;
}

status_t get_accounts(accounts_table_t *accounts)
{
	status_t error;

	memset(accounts->accounts, 0, ACCOUNT_BUCKETS * sizeof *accounts->accounts);

	int file = open("config/accounts", O_RDONLY, 0);
	if (file < 0)
	{
		error = FILE_OPEN_ERROR;
		goto exit0;
	}

	string_t num_entries_str;
	string_initialize(&num_entries_str);
	error = read_line_strip_endings(file, &num_entries_str);
	if (error)
	{
		goto exit1;
	}
	int records = atoi(string_c_str(&num_entries_str));

	string_t username, password;
	string_initialize(&username);
	string_initialize(&password);

	int i;
	for (i = 0; i < records; i++)
	{
		char_vector_clear(&username);
		char_vector_clear(&password);
		
		error = read_line_strip_endings(file, &username);
		if (error)
		{
			goto exit_error1;
		}
		
		error = read_line_strip_endings(file, &password);
		if (error)
		{
			goto exit_error1;
		}

		account_t *account = malloc(sizeof *account);
		if (account == NULL)
		{
			goto exit_error1;
		}

		account->username = strdup(string_c_str(&username));
		account->password = strdup(string_c_str(&password));

		size_t hash_val = accounts_hash(account->username);
		account->next = accounts->accounts[hash_val];
		accounts->accounts[hash_val] = account;

		printf("%s: %s\n", account->username, account->password);
	}

	goto exit2;

exit_error1:
	free_accounts(accounts);
exit2:
	string_uninitialize(&password);
	string_uninitialize(&username);
exit1:
	close(file);
exit0:
	return error;
}

status_t get_account_by_username(accounts_table_t *accounts, char *username, account_t **account)
{
	size_t hash_val = accounts_hash(username);
	*account = accounts->accounts[hash_val];
	while (*account != NULL && !bool_strcmp((*account)->username, username))
	{
		*account = (*account)->next;
	}

	return SUCCESS;
}

status_t free_accounts(accounts_table_t *accounts)
{
	size_t i;
	for (i = 0; i < ACCOUNT_BUCKETS; i++)
	{
		account_t *head = accounts->accounts[i];
		while (head != NULL)
		{
			free(head->username);
			free(head->password);
			account_t *tmp = head->next;
			free(head);
			head = tmp;
		}
	}
}

void *client_handler(void *void_args)
{
	user_session_t *session = (user_session_t *) void_args;
	status_t error;

	session->directory = realpath(".", NULL);
	if (session->directory == NULL)
	{
		error = REALPATH_ERROR;
		goto exit0;
	}

	error = send_response(session->command_sock, SERVICE_READY,
		"Ready. Please send USER.", session->log);
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
		error = read_line_strip_endings(session->command_sock, &command);
		if (!error)
		{
			size_t len;
			string_t *split = string_split_skip_consecutive(&command, ' ', &len, 1);
			char *c_str = string_c_str(split + 0);
			
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
	} while (!error && !done);

exit2:
	string_uninitialize(&command);
exit1:
	free(session->directory);
exit0:
	printf("Client quitting.\n");
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

	get_account_by_username(session->accounts, string_c_str(&username), &session->account);
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
}

status_t handle_epsv_command(user_session_t *session, string_t *args, size_t len)
{
}

status_t handle_port_command(user_session_t *session, string_t *args, size_t len)
{
}

status_t handle_eprt_command(user_session_t *session, string_t *args, size_t len)
{
}

status_t handle_retr_command(user_session_t *session, string_t *args, size_t len)
{
}

status_t handle_pwd_command(user_session_t *session, string_t *args, size_t len)
{
	return send_257(session);
}

status_t handle_list_command(user_session_t *session, string_t *args, size_t len)
{
}

status_t handle_help_command(user_session_t *session, string_t *args, size_t len)
{
}

status_t handle_unrecognized_command(user_session_t *session, string_t *args, size_t len)
{
}

status_t send_response(int sock, char *code, char *message, log_t *log)
{
	status_t error;

	string_t response;
	string_initialize(&response);
	string_assign_from_char_array_with_size(&response, code, 3);
	string_concatenate_char_array_with_size(&response, " ", 1);
	string_concatenate_char_array(&response, message);
	string_concatenate_char_array(&response, "\r\n");

	error = send_string(sock, &response, log->log_file);
	if (error)
	{
		goto exit0;
	}

exit0:
	string_uninitialize(&response);
	return error;
}

uint8_t bool_memcmp(char *s1, char *s2, size_t n)
{
	return memcmp(s1, s2, n) == 0;
}

uint8_t bool_strcmp(char *s1, char *s2)
{
	return strcmp(s1, s2) == 0;
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
