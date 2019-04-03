#ifndef _CLIENT_H_
#define _CLIENT_H_


#define OUT_STAT(tStart, tEnd, tx, rx, err) \
		printf("\n\033[7m");\
		printf("Time spent: %0.1f sec.\n", difftime(tEnd, tStart));\
		printf("TX:%d RX:%d\n",tx,rx);\
		if ((tx>rx)) printf("Compression: %1.2f%%\n", (100 - (rx*100.0/tx)));\
		if (err) printf("ERROR: %d\n",err);\
		printf("\033[0m\n");

#pragma pack(push,1)
struct conf_t {
	const char *host;
	unsigned char compressionType;
	unsigned int chunk_size;
	int compressionLevel;
	unsigned char client_model;
	unsigned int input_fsize;
	const char *input_filename;
} conf;

typedef struct ClientThreadsParametes_t {
	int c_socket;
	unsigned char c_compressionType;
	unsigned int c_fsize;
	unsigned int c_chunk;
	const char *c_filename;
	unsigned char c_model;
	void*	sharedStat;
} ClientThreadsParametes;
#pragma pack(pop)

typedef struct data_stat_t {
	unsigned int rx_bytes;
	unsigned int tx_bytes;
	int error;
} DataStat;

DataStat *gds;

volatile sig_atomic_t clnt_incomingSignal;
void clnt_incomingSignal_parse(int signum);
int bringUpServer(int socket,unsigned int dataSize);
DataStat *txThread(void *params);
DataStat *rxThread(void *params);
DataStat *RxTxThread(void *params);
#endif //#ifndef _CLIENT_H_
