#ifndef __FTP_H__
#define __FTP_H__

#include "status_t.h"
#include "string_t.h"

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

status_t send_string(int sock, string_t *s, int log_file);

status_t read_line_strip_endings(int socket, string_t *line);

/**
  * reads from socket socket until a '\r\n' sequence is reached
  * @param socket - socket from which to read
  * @param line   - out param; the string into which to read. Must be initialized
  */
status_t read_single_line(int socket, string_t *response);

/**
  *	reads a single character from the socket
  * @param socket - socket from which to read
  * @param c      - pointer to character into which to read
  */
status_t read_single_character(int socket, char *c);
#endif
