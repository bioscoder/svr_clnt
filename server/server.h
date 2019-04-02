#ifndef _SERVER_H_
#define _SERVER_H_

#define MAX_ONLINE_CLIENTS	10

#define RAW_DEFLATE_WINDOW	-15
#define ZLIB_DEFLATE_WINDOW	15
#define GZIP_WINDOW					(15+16)

#include "custom_zlib/zlib.h"
#include <lzma.h>

typedef struct ClientsOnline_t
{
	unsigned char inUse;
	int c_socket;
	struct sockaddr_storage* c_addr;
	char c_ip[INET_ADDRSTRLEN];
	tCompression c_compressionType;
	int c_compressionLevel;
	unsigned int c_targetSize;
	int c_error;
	unsigned int c_rxSize;
	unsigned int c_txSize;
	z_stream rStream;
	z_stream zStream;
	lzma_stream lzStream;
	z_stream gzStream;
}ClientsOnline;

volatile sig_atomic_t srv_incomingSignal;
void srv_incomingSignal_parse(int signum);
unsigned char getInfoFromHeader(char *input, unsigned int input_size, ClientsOnline* output);
unsigned int processAndSend(int sock, char *data, unsigned int dataLen, void *params);
void showCstat(ClientsOnline* client);
#endif //#ifndef _SERVER_H_
