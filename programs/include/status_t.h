#ifndef __STATUS_T_H__
#define __STATUS_T_H__

#include "log.h"

typedef enum
{
	SUCCESS = 0,
	BAD_COMMAND_LINE,
	FILE_OPEN_ERROR,
	FILE_WRITE_ERROR,
	SOCKET_OPEN_ERROR,
	SOCKET_WRITE_ERROR,
	SOCKET_READ_ERROR,
	CONNECTION_ERROR,
	BIND_ERROR,
	LISTEN_ERROR,
	ACCEPT_ERROR,
	SOCK_NAME_ERROR,
	HOST_ERROR,
	MEMORY_ERROR,
	ACCEPTING_ERROR,
	LOG_IN_ERROR,
	SERVICE_AVAILIBILITY_ERROR,
	GET_NAME_ERROR,
	TIME_GET_ERROR,
	TIME_STRING_ERROR,
	NON_FATAL_ERROR,
	PTHREAD_CREATE_ERROR,
	SOCKET_EOF,
	REALPATH_ERROR,
	LOCK_INIT_ERROR,
	FILE_READ_ERROR,
	CONFIG_FILE_ERROR,
	DIR_OPEN_ERROR,
} status_t;

/**
  * given an error code, returns a generic, stock error message for that type
  * @param error the error flag
  */
char *get_error_message(status_t error);

/**
  * given an error code, prints a generic, stock error message for that type
  * @param error the error flag
  */
void print_error_message(status_t error);

/**
  * Prints the given error message to the screen and also writes it to the given
  * log
  * @param error - the error code
  * @param log - the log file to which to write
  */
//void print_and_log_error_message(status_t error, log_t *log);

#endif
