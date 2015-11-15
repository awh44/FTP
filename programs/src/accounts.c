#include <fcntl.h>
#include <string.h>

#include "accounts.h"
#include "string_t.h"

/**
  * Hash function for the accounts hash table. Uses the djb2 algorithm - see
  * http://www.cse.yorku.ca/~oz/hash.html
  * @param str - the string (username) to be hashed
  */
size_t hash(char *str);

/**
  * Performs a hash and then mods by the number of buckets available in the hash
  * table
  * @param str - the string/username to be hashed
  */
size_t accounts_hash(char *str);

status_t get_accounts(char *filename, accounts_table_t *accounts)
{
	status_t error;

	memset(accounts->accounts, 0, ACCOUNT_BUCKETS * sizeof *accounts->accounts);

	int file = open(filename, O_RDONLY, 0);
	if (file < 0)
	{
		error = FILE_OPEN_ERROR;
		goto exit0;
	}

	string_t num_entries_str;
	string_initialize(&num_entries_str);
	error = read_line_strip_endings(file, &num_entries_str);
	if (error)
	{
		goto exit1;
	}
	int records = atoi(string_c_str(&num_entries_str));

	string_t username, password;
	string_initialize(&username);
	string_initialize(&password);

	int i;
	for (i = 0; i < records; i++)
	{
		char_vector_clear(&username);
		char_vector_clear(&password);

		error = read_line_strip_endings(file, &username);
		if (error)
		{
			goto exit_error1;
		}

		error = read_line_strip_endings(file, &password);
		if (error)
		{
			goto exit_error1;
		}

		account_t *account = malloc(sizeof *account);
		if (account == NULL)
		{
			goto exit_error1;
		}

		account->username = strdup(string_c_str(&username));
		account->password = strdup(string_c_str(&password));

		size_t hash_val = accounts_hash(account->username);
		account->next = accounts->accounts[hash_val];
		accounts->accounts[hash_val] = account;
	}

	goto exit2;

exit_error1:
	free_accounts(accounts);
exit2:
	string_uninitialize(&password);
	string_uninitialize(&username);
exit1:
	string_uninitialize(&num_entries_str);
	close(file);
exit0:
	return error;
}

status_t get_account_by_username(accounts_table_t *accounts, char *username, account_t **account)
{
	size_t hash_val = accounts_hash(username);
	*account = accounts->accounts[hash_val];
	while (*account != NULL && !bool_strcmp((*account)->username, username))
	{
		*account = (*account)->next;
	}

	return SUCCESS;
}

status_t free_accounts(accounts_table_t *accounts)
{
	size_t i;
	for (i = 0; i < ACCOUNT_BUCKETS; i++)
	{
		account_t *head = accounts->accounts[i];
		while (head != NULL)
		{
			free(head->username);
			free(head->password);
			account_t *tmp = head->next;
			free(head);
			head = tmp;
		}
	}
}

size_t hash(char *str)
{
    size_t hash_val = 5381;
    char c;

    while ((c = *str++))
    {
        hash_val = ((hash_val << 5) + hash_val) + c;
    }

    return hash_val;
}

size_t accounts_hash(char *str)
{
	return hash(str) % ACCOUNT_BUCKETS;
}
