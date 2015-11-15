#include <dirent.h>

#include "accounts.h"
#include "ftp.h"
#include "log.h"
#include "server.h"

#define CONFIG_FILE ".ftpdlog"
#define LOG_DIR_PARAM "logdirectory"
#define NUM_LOGS_PARAM "numlogfiles"
#define NEXT_LOG_NUM_PARAM "nextlognum"
#define USER_FILE_PARAM "usernamefile"
#define PORT_MODE_PARAM "port_mode"
#define PASV_MODE_PARAM "pasv_mode"
#define DEFAULT_LOG_DIR "logs"

/**
  * Handles the "port_mode" and "pasv_mode" parameters of the configuration
  * file
  * @param server_val - the flag on the server object to be set
  * @param value      - the value given for the parameter in the cnofig file
  * @param param      - the name of the parameter (either "pasv_mode" or "port_mode")
  */
status_t port_pasv_param(int8_t *server_val, char *value, char *param);

status_t initialize_server(server_t *server)
{
	status_t error = SUCCESS;
	server->accounts = NULL;
	server->log = NULL;
	server->ip4 = NULL;
	server->ip6 = NULL;

	FILE *file = fopen(CONFIG_FILE, "r+");
	if (file == NULL)
	{
		printf("Could not open log file.\n");
		error = CONFIG_FILE_ERROR;
		goto exit0;
	}

	char *log_dir = NULL; //so it's safe to free
	int files_to_keep = -1;
	long int next_log_num_pos = -1;
	int next_log_num = -1;
	server->port_enabled = -1;
	server->pasv_enabled = -1;

	char *line = NULL; //make sure that getline allocates space for the line
	size_t length = 0;
	ssize_t chars_read;
	while ((chars_read = getline(&line, &length, file)) > 0)
	{
		//Will never be a problem dereferencing line[0] because line will always
		//contain at least '\n'
		if (line[0] != '#')
		{
			//Remove the '\n'
			line[chars_read - 1] = '\0';
			char *value = line;
			char *param = strsep(&value, "=");
			if (value == NULL)
			{
				//If value is set to NULL, The line did not contain an equals sign,
				//so return an error
				printf("Parameter in configuration file is missing associated value.\n");
				error = CONFIG_FILE_ERROR;
				goto exit1;
			}

			if (bool_strcmp(param, LOG_DIR_PARAM))
			{
				log_dir = strdup(value);
			}
			else if (bool_strcmp(param, NUM_LOGS_PARAM))
			{
				files_to_keep = atoi(value);
				if (files_to_keep <= 0 || files_to_keep > MAX_LOG_FILES)
				{
					printf("The '%s' parameter must be greater than 0 and less than or equal to %d.\n", NUM_LOGS_PARAM, MAX_LOG_FILES);
					error = CONFIG_FILE_ERROR;
					goto exit1;
				}
			}
			else if (bool_strcmp(param, NEXT_LOG_NUM_PARAM))
			{
				//Subtract to the beginning of the number (with 1 extra because of the '\n')
				if (strlen(value) != 3 ||  (next_log_num = atoi(value)) < 0 || next_log_num >= MAX_LOG_FILES)
				{
					printf("The '%s' parameter has been corrupted. It must be three digits and greater than or equal to 000 and less than %d.\n", NEXT_LOG_NUM_PARAM, MAX_LOG_FILES);
					error = CONFIG_FILE_ERROR;
					goto exit1;
				}
				next_log_num_pos = ftell(file) - LOG_FILE_EXT_LEN - 1;
			}
			else if (bool_strcmp(param, USER_FILE_PARAM))
			{
				accounts_table_t *accounts = malloc(sizeof *server->accounts);
				if (accounts == NULL)
				{
					error = MEMORY_ERROR;
					goto exit1;
				}

				error = get_accounts(value, accounts);
				if (error)
				{
					free(accounts);
					print_error_message(error);
					error = CONFIG_FILE_ERROR;
					goto exit1;
				}
				server->accounts = accounts;
			}
			else if (bool_strcmp(param, PORT_MODE_PARAM))
			{
				error = port_pasv_param(&server->port_enabled, value, PORT_MODE_PARAM);
				if (error)
				{
					goto exit1;
				}
			}
			else if (bool_strcmp(param, PASV_MODE_PARAM))
			{
				error = port_pasv_param(&server->pasv_enabled, value, PASV_MODE_PARAM);
				if (error)
				{
					goto exit1;
				}
			}
			else
			{
				printf("Unrecognized parameter ('%s') in the configuration file.\n", param);
				error = CONFIG_FILE_ERROR;
				goto exit1;
			}
		}
	}

	if (log_dir == NULL)
	{
		log_dir = strdup(DEFAULT_LOG_DIR);
	}

	log_t *log = malloc(sizeof *log);
	if (log == NULL)
	{
		error = MEMORY_ERROR;
		goto exit1;
	}
	error = open_log_file_in_dir(log, log_dir, files_to_keep, next_log_num, 1);
	if (error)
	{
		printf("Error opening log file.\n");
		free(log);
		goto exit1;
	}
	server->log = log;
	if (fseek(file, next_log_num_pos, SEEK_SET) < 0)
	{
		error = CONFIG_FILE_ERROR;
		goto exit1;
	}
	next_log_num = (next_log_num + 1) % MAX_LOG_FILES;
	fprintf(file, "%03d", next_log_num);

	if (server->port_enabled < 0 || server->pasv_enabled < 0)
	{
		printf("The '%s' and '%s' parameters must both be set in the config file.", PORT_MODE_PARAM, PASV_MODE_PARAM);
		error = CONFIG_FILE_ERROR;
		goto exit1;
	}

	if (!server->port_enabled && !server->pasv_enabled)
	{
		printf("Either PORT or PASV must be enabled.\n");
		error = CONFIG_FILE_ERROR;
		goto exit1;
	}

	char ips[] = "Getting local ips.";
	error = write_log(server->log, ips, sizeof ips);
	if (error)
	{
		goto exit1;
	}
	error = get_ips(&server->ip4, &server->ip6);
	if (error)
	{
		goto exit1;
	}

exit1:
	free(log_dir);
	free(line);
	fclose(file);
exit0:
	return error;
}

void free_server(server_t *server)
{
	if (server->accounts != NULL)
	{
		free_accounts(server->accounts);
		free(server->accounts);
	}

	if (server->log != NULL)
	{
		close_log_file(server->log);
		free(server->log);
	}

	free(server->ip4);
	free(server->ip6);
}

status_t port_pasv_param(int8_t *server_val, char *value, char *param)
{
	status_t error = SUCCESS;

	if (bool_strcmp(value, "YES"))
	{
		*server_val = 1;
	}
	else if (bool_strcmp(value, "NO"))
	{
		*server_val = 0;
	}
	else
	{
		printf("The '%s' parameter must be either 'YES' or 'NO'.\n", param);
		error = CONFIG_FILE_ERROR;
	}

	return error;
}
