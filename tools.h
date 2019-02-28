#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#if 0
#include "zlib/zlib.h"
#endif

#define TARGET_PORT 		3134
#define CLIENTS_IFACE_ADDR	INADDR_ANY
#define TARGET_ADDR 		"127.0.0.1"

#define MAGIC_START_KEY	0xDEAD8086
#define MAGIC_END_KEY	0x8086BEAF

#define BUFFER_SIZE		1024
#define Z_CHUNK		BUFFER_SIZE

typedef struct __attribute__((__packed__)) WorkerParametes_t {
	int w_socket;
	unsigned char w_compressionType;
	long fsize;
} WorkerParameters;

typedef struct __attribute__((__packed__)) ClientThreadsParametes_t {
	int c_socket;
	int c_wr_pipe;
	int c_rd_pipe;
	unsigned char c_compressionType;
	long c_fsize;
	//char *c_flink;
	FILE * c_flink;
	const char *c_filename;
} ClientThreadsParametes;



struct __attribute__((__packed__)) conf_t {
	const char *host;	/*-H. server address*/
	FILE * outFile;		/*-I. Input file for server*/
	unsigned char arch_type;		/*-T */
	long input_fsize;
	const char *input_filename;
} conf;

typedef struct __attribute__((__packed__)) MagicToken_t {
	int start_key;
	unsigned char compressionType;
	long nextdatasizes;
	int end_key;
} MagicToken;



volatile sig_atomic_t clnt_incomingSignal = 0;
void clnt_incomingSignal_parse(int signum)
{
	clnt_incomingSignal = signum;
}
volatile sig_atomic_t srv_incomingSignal = 0;
void srv_incomingSignal_parse(int signum)
{
	srv_incomingSignal = signum;
}
