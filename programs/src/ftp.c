#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "ftp.h"
#include "log.h"
#include "status_t.h"
#include "string_t.h"

status_t send_string(int sock, string_t *s, log_t *log_file)
{
	status_t error = SUCCESS;
	if (write(sock, string_c_str(s), string_length(s)) < 0)
	{
		error = SOCKET_WRITE_ERROR;
		goto exit0;
	}

	error = write_sent_message_to_log(log_file, s);
	if (error)
	{
		goto exit0;
	}

exit0:
	return error;
}

status_t read_line_strip_endings(int socket, string_t *line)
{
	status_t error;
	error = read_single_line(socket, line);
	if (error)
	{
		goto exit0;
	}

	char_vector_pop_back(line);
	char_vector_pop_back(line);

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

status_t read_single_character(int socket, char *c)
{
	ssize_t bytes_read;
	if ((bytes_read = read(socket, c, 1)) < 0)
	{
		return SOCKET_READ_ERROR;
	}
	else if (bytes_read == 0)
	{
		return SOCKET_EOF;
	}

	return SUCCESS;
}

status_t parse_ip_and_port(string_t *split, size_t len, string_t *host, uint16_t *port)
{
	status_t error = SUCCESS;
	if (len < 6)
	{
		error = NON_FATAL_ERROR;
		goto exit_error1;
	}

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

	goto exit0;

exit_error1:
	for (i = 0; i < 4; i++)
	{
		string_uninitialize(split + i);
	}

exit0:
	string_uninitialize(split + len - 2);
	string_uninitialize(split + len - 1);
	free(split);
	return error;
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

status_t get_ips(char **ip4, char **ip6)
{
	status_t error = SUCCESS;
	*ip4 = NULL;
	*ip6 = NULL;
	
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

		if (ifa->ifa_addr->sa_family == AF_INET && *ip4 == NULL)
		{
			//try to set ip4 from ifa, ignoring the address if it's the loopback
			//address
			error = try_set_hostname(ifa, ip4, sizeof(struct sockaddr_in),
				"127.0.0.1");
			if (error)
			{
				free(*ip6);
				goto exit1;
			}
		}
		else if (ifa->ifa_addr->sa_family == AF_INET6 && *ip6 == NULL)
		{
			//try to set ip6 from ifa, ignoring the address if it's the loopback
			//address
			error = try_set_hostname(ifa, ip6, sizeof(struct sockaddr_in6),
				"::1");
			if (error)
			{
				free(*ip4);
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
