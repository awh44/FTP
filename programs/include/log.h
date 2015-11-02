#ifndef __LOG_H__
#define __LOG_H__

#include "status_t.h"
#include "string_t.h"

/**
  * opens the file to be ussed for logging at file name filename
  * @param fd       - out param; the file desrcriptor for the log file
  * @param filename - the file name to use for the log file
  */
status_t open_log_file(int *fd, char *filename);

/**
  * logs the message of length into the log file given by session
  * @param log_file - the file to which the message will be written
  * @param message - the messsage to write to the log
  * @param length  - the length of the message
  */
status_t write_log(int log_file, char *message, size_t length);

/**
  * write a "received" message to the log, with the received data
  * @param session - file to which to log
  * @param message - the message received/to be written
  */
status_t write_received_message_to_log(int log_file, string_t *message);

/**
  * write a "sent" message to the log, with the send data
  * @param session - file to which to log
  * @param message - the message sent/to be written
  */
status_t write_sent_message_to_log(int log_file, string_t *message);
    
/** 
  * write a message to the log, prepended by "prepend" of length size
  * @param session - file to which to log
  * @param message - the message to be written
  * @param prepend - some data to prepend to the message
  * @param size    - the length of prepend
  */
status_t prepend_and_write_to_log(int log_file, string_t *message, char
    *prepend, size_t size);

#endif