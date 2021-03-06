#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define LOG_FILE_NAME "/logfile."
#define LOG_FILE_NAME_LEN (sizeof LOG_FILE_NAME - 1)

#include "log.h"
#include "status_t.h"

status_t open_log_file_clobber_opt(log_t *log, char *filename, uint8_t threaded, uint8_t clobber);

status_t open_log_file(log_t *log, char *filename, uint8_t threaded)
{
	return open_log_file_clobber_opt(log, filename, threaded, 0);
}

status_t open_log_file_clobber_opt(log_t *log, char *filename, uint8_t threaded, uint8_t clobber)
{
	//Create the file if it doesn't exist, and always append new logs to the end
	//of the file
	if ((log->log_file = open(filename, O_WRONLY | O_CREAT | (clobber ? O_TRUNC : O_APPEND), 0600)) < 0)
	{
		return FILE_OPEN_ERROR;
	}

	if (threaded)
	{
		log->lock = malloc(sizeof *log->lock);
		if (pthread_mutex_init(log->lock, NULL) != 0)
		{
			close(log->log_file);
			printf("Could not initialize pthread mutex.\n");
			return LOCK_INIT_ERROR;
		}
	}
	else
	{
		log->lock = NULL;
	}

	return SUCCESS;

}

status_t open_log_file_in_dir(log_t *log, char *dirname, int files_to_keep, int next_log_num, uint8_t threaded)
{
	status_t error = SUCCESS;

	string_t generic_filename;
	string_initialize(&generic_filename);
	string_assign_from_char_array(&generic_filename, dirname);
	string_concatenate_char_array(&generic_filename, LOG_FILE_NAME);
	
	//plus 1 for the '\0'
	char next_string[LOG_FILE_EXT_LEN + 1];
	sprintf(next_string, "%03d", next_log_num);

	string_t opening_name;
	string_initialize(&opening_name);
	char_vector_copy(&opening_name, &generic_filename);
	string_concatenate_char_array(&opening_name, next_string);

	error = open_log_file_clobber_opt(log, string_c_str(&opening_name), threaded, 1);
	if (error)
	{
		goto exit0;
	}

	if (files_to_keep > 0)
	{
		sprintf(next_string, "%03d", (next_log_num - files_to_keep + MAX_LOG_FILES) % MAX_LOG_FILES);
		string_concatenate_char_array(&generic_filename, next_string);
		unlink(string_c_str(&generic_filename));
	}

exit0:
	string_uninitialize(&opening_name);
	string_uninitialize(&generic_filename);
	return error;
}

status_t close_log_file(log_t *log)
{
	close(log->log_file);
	if (log->lock)
		pthread_mutex_destroy(log->lock);
	return SUCCESS;
}

status_t write_log(log_t *log, char *message, size_t length)
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

	if (log->lock)
		pthread_mutex_lock(log->lock);

	if (write(log->log_file, time_val_string, time_len) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit1;
	}

	if (write(log->log_file, message, length) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit1;
	}

	if (write(log->log_file, "\n", 1) < 0)
	{
		error = FILE_WRITE_ERROR;
		goto exit1;
	}

exit1:
	if (log->lock)
		pthread_mutex_unlock(log->lock);
exit0:
	return error;
}

status_t write_received_message_to_log(log_t *log, string_t *message)
{
	char received[] = "Received: ";
	return prepend_and_write_to_log(log, message, received,
		sizeof received - 1);
}

status_t write_sent_message_to_log(log_t *log, string_t *message)
{
	char received[] = "Sent: ";
	return prepend_and_write_to_log(log, message, received,
		sizeof received - 1);
}

status_t prepend_and_write_to_log(log_t *log, string_t *message, char
	*prepend, size_t size)
{
	status_t error = SUCCESS;

	string_t final_message;
	string_initialize(&final_message);
	string_assign_from_char_array_with_size(&final_message, prepend, size);
	string_concatenate(&final_message, message);

	error = write_log(log, string_c_str(&final_message),
		string_length(&final_message));
	if (error)
	{
		goto exit0;
	}

exit0:
	string_uninitialize(&final_message);
	return error;

}
