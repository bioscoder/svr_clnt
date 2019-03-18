#ifndef _CLIENT_H_
#define _CLIENT_H_

struct __attribute__((__packed__)) conf_t {
	const char *host;			/*-H. server address*/
	FILE * outFile;				/*-I. Input file for server*/
	unsigned char arch_type;	/*-T */
	unsigned int input_fsize;
	const char *input_filename;
} conf;

typedef struct __attribute__((__packed__)) ClientThreadsParametes_t {
	int c_socket;
	int c_wr_pipe;
	int c_rd_pipe;
	unsigned char c_compressionType;
	unsigned int c_fsize;
	FILE * c_flink;
	const char *c_filename;
} ClientThreadsParametes;

volatile sig_atomic_t clnt_incomingSignal;
void clnt_incomingSignal_parse(int signum);

void *client_thread(void *params);

#endif //#ifndef _CLIENT_H_
