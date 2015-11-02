#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#include "log.h"
#include "status_t.h"

status_t open_log_file(int *fd, char *filename)
{
	//Create the file if it doesn't exist, and always append new logs to the end
	//of the file
	if ((*fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0600)) < 0)
	{
		printf("Could not open log file.\n");
		return FILE_OPEN_ERROR;
	}

	return SUCCESS;
}

status_t write_log(int log_file, char *message, size_t length)
{
	status_t error = SUCCESS;

	time_t time_val = time(NULL);
	if (time_val < 0)
	{
		error = TIME_GET_ERROR;
		goto exit0;
	}

	char *time_val_string = ctime(&time_val);
	if (time_val_string == NULL)
	{
		error = TIME_STRING_ERROR;
		goto exit0;
	}
	size_t time_len = strlen(time_val_string);
	time_val_string[time_len - 1] = ' ';

	if (write(log_file, time_val_string, time_len) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit0;
	}

	if (write(log_file, message, length) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit0;
	}

	if (write(log_file, "\n", 1) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit0;
	}

exit0:
	return error;
}

status_t write_received_message_to_log(int log_file, string_t *message)
{
	char received[] = "Received: ";
	return prepend_and_write_to_log(log_file, message, received,
		sizeof received - 1);
}

status_t write_sent_message_to_log(int log_file, string_t *message)
{
	char received[] = "Sent: ";
	return prepend_and_write_to_log(log_file, message, received,
		sizeof received - 1);
}

status_t prepend_and_write_to_log(int log_file, string_t *message, char
	*prepend, size_t size)
{
	status_t error = SUCCESS;

	string_t final_message;
	string_initialize(&final_message);
	string_assign_from_char_array_with_size(&final_message, prepend, size);
	string_concatenate(&final_message, message);

	error = write_log(log_file, string_c_str(&final_message),
		string_length(&final_message));
	if (error)
	{
		goto exit0;
	}

exit0:
	string_uninitialize(&final_message);
	return error;

}