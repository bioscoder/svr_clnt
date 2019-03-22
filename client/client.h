#ifndef _CLIENT_H_
#define _CLIENT_H_


#define OUT_STAT(tStart, tEnd, tx, rx) \
		printf("\033[7m");\
		printf("Time spent: %0.1f sec.\n", difftime(tEnd, tStart));\
		printf("TX:%d RX:%d\n",tx,rx);\
		if ((tx>=rx)) printf("Compression: %1.2f%%\n", (100 - (rx*100.0/tx)));\
		printf("\033[0m\n");


struct __attribute__((__packed__)) conf_t {
	const char *host;
	unsigned char arch_type;
	unsigned char client_model;
	unsigned int input_fsize;
	const char *input_filename;
} conf;

typedef struct __attribute__((__packed__)) ClientThreadsParametes_t {
	int c_socket;
	unsigned char c_compressionType;
	unsigned int c_fsize;
	const char *c_filename;
	unsigned char c_model;
} ClientThreadsParametes;

typedef struct data_stat_t {
	unsigned int rx_bytes;
	unsigned int tx_bytes;
} DataStat;

DataStat *gds;

volatile sig_atomic_t clnt_incomingSignal;
void clnt_incomingSignal_parse(int signum);
int bringUpServer(int socket,unsigned int dataSize);
DataStat *txThread(void *params);
DataStat *rxThread(void *params);
DataStat *RxTxThread(void *params);
#endif //#ifndef _CLIENT_H_
