/*typedef uint64_t status_t;
#define SUCCESS                     0
#define BAD_COMMAND_LINE            1
#define FILE_OPEN_ERROR             2
#define FILE_WRITE_ERROR            3
#define SOCKET_OPEN_ERROR           4
#define SOCKET_WRITE_ERROR          5
#define SOCKET_READ_ERROR           6
#define CONNECTION_ERROR            7
#define BIND_ERROR                  8
#define LISTEN_ERROR                9
#define ACCEPT_ERROR               10
#define SOCK_NAME_ERROR            11
#define HOST_ERROR                 12
#define MEMORY_ERROR               13
#define ACCEPTING_ERROR            14
#define LOG_IN_ERROR               15
#define SERVICE_AVAILIBILITY_ERROR 16
#define GET_NAME_ERROR             17
#define TIME_GET_ERROR             18
#define TIME_STRING_ERROR          19
#define NON_FATAL_ERROR            20
*/

typedef enum
{
	SUCCESS = 0,
	BAD_COMMAND_LINE,
	FILE_OPEN_ERROR,
	FILE_WRITE_ERROR,
	SOCKET_OPEN_ERROR,
	SOCKET_WRITE_ERROR,
	SOCKET_READ_ERROR,
	CONNECTION_ERROR,
	BIND_ERROR,
	LISTEN_ERROR,
	ACCEPT_ERROR,
	SOCK_NAME_ERROR,
	HOST_ERROR,
	MEMORY_ERROR,
	ACCEPTING_ERROR,
	LOG_IN_ERROR,
	SERVICE_AVAILIBILITY_ERROR,
	GET_NAME_ERROR,
	TIME_GET_ERROR,
	TIME_STRING_ERROR,
	NON_FATAL_ERROR,
} status_t;

void print_error_message(status_t error);
