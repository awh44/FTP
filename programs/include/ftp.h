#ifndef __FTP_H__
#define __FTP_H__

#include <ifaddrs.h>

#include "log.h"
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

#define PORT_DIVISOR 256

status_t send_string(int sock, string_t *s, log_t *log);

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

status_t parse_ip_and_port(string_t *split, size_t len, string_t *host, uint16_t *port);

/**
  * makes a connection to the given host on the given port, using socket sock
  * @param sock - out param; the socket over which the connection will be made
  * @param host - the host name to which to connect
  * @param port - the port number to which to connect (still in host order)
  */
status_t make_connection(int *sock, char *host_str, uint16_t port);

/**
  * gets the IPv4 and IPv6 IP addresses, if available, by walking the ifaddrs
  * list and sets them appropriately in session
  * @param ip4 - pointer to the ip4 pointer to allocate and set
  * @param ip6 - pointer to the ip6 pointer to allocate and set
  */
status_t get_ips(char **ip4, char **ip6);

/**
  * helper function for get_ips; if the host can be retrieved from ifa and does
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
  * helper function for port_command; sets up a listening socket in the current
  * session, using internet protocol af and address address, setting
  * listen_socket and port appropriately when finished
  */
status_t set_up_listen_socket(int *listen_socket, uint16_t
	*listen_port, int af, char *address);

/**
  * Takes the C string address and the port number port and creates a comma
  * delimited address of the form h1,h2,h3,h4,p1,p2 and concatenates it to args
  * (note that this means that text can already exist in args and it will not be
  * overwritten)
  */
void create_comma_delimited_address(string_t *args, char *address, uint16_t port);

/**
  * Performs a strcmp and returns true if the result is 0, false otherwise
  * @param s1 - the first string to compare
  * @param s2 - the second string to compare
  * @return true if s1's bytes are equal to s2's bytes, up to the first null
  * 	terminator
  */
uint8_t bool_strcmp(char *s1, char *s2);

#endif
