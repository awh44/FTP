#include "ftp.h"
#include "status_t.h"
#include "string_t.h"

status_t send_string(int sock, string_t *s, int log_file)
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
