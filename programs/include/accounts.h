#ifndef __ACCOUNTS_H_
#define __ACCOUNTS_H_

#include "status_t.h"

/**
  *	Structure used to represent a user account. Contains next field to be used
  *	as a linked list.
  * username - account username
  * password - account password
  * next - the next element in the linked list
  */
typedef struct account
{
	char *username;
	char *password;
	struct account *next;
} account_t;

#define ACCOUNT_BUCKETS 512
/**
  * A hash table data structure for all of the accounts
  * accounts - the hash table itself
  */
typedef struct
{
	account_t *accounts[ACCOUNT_BUCKETS];
} accounts_table_t;

/**
  * Reads the accounts file at location filename and puts the
  * user account information into the accounts table. The format of the accounts
  * file is as follows:
  *		The first line is the number of accounts in the file
  *		After that, the first user's username sits on a line, followed by CRLF,
  *			and then that user's password sits on the next line, followed by
  *			CRLF
  *		The next user's username sits on the following line, and this repeats
  *			for all the users in the file
  * @param filename - the accounts file to use
  * @param accounts - out param; will hold all the account information upon
  * 	completion
  */
status_t get_accounts(char *filename, accounts_table_t *accounts);

/**
  *	Finds the user with the given username in the accounts table and makes
  *	*account point to it, making it point to NULL if there is no matching user.
  * @param accounts - accounts table to search
  * @param username - username for which to search
  * @param accoutn - out param; pointer to the pointer which will hold the
  *		account reference
  */
status_t get_account_by_username(accounts_table_t *accounts, char *username, account_t **account);

/**
  * Frees all of the account data associated with the accounts table
  * @param accounts - the table to be freed
  */
status_t free_accounts(accounts_table_t *accounts);

#endif
