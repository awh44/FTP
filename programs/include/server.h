#ifndef __SERVER_H__
#define __SERVER_H__

#include "accounts.h"
#include "log.h"
#include "status_t.h"

/**
  * Structure for holding the server configuration/information. Contains a
  * reference to the accounts table, the log file, the address of the server,
  * and various flags.
  * accounts - the table of accounts on the server
  * log - the log file on the server
  * ip4 - the IPv4 address of the server
  * ip6 - the IPv6 address of the server
  */
typedef struct
{
	accounts_table_t *accounts;
	log_t *log;
	char *ip4;
	char *ip6;
	int8_t port_enabled;
	int8_t pasv_enabled;
} server_t;

/**
  * Initializes the server object, doing so in part by reading the configuration
  * file at .ftpdlog in the current directory Note that if an error is returned,
  * the server might end up in a "half-initialized" state, e.g., some data
  * structures might be malloc'd but not set up correctly, so if an error is
  * returned, the free_server function should be called immediately, because the
  * function will correctly handle half-initialized servers
  * @param server - the server object to initialize
  */
status_t initialize_server(server_t *server);

/**
  * Frees all memory, closes all files, etc. associated with the server.
  * @param server - the server object to free
  */
void free_server(server_t *server);
#endif
