#ifndef _SERVER_H_
#define _SERVER_H_

#define MAX_ONLINE_CLIENTS	10
#define ZLIB_DEFLATE_LEVEL	6

#include "zlib/zlib.h"

typedef struct ClientsOnline_t
{
	unsigned char inUse;
	int c_socket;
	struct sockaddr_storage* c_addr;
	char c_ip[INET_ADDRSTRLEN];
	tCompression c_compressionType;
	unsigned int c_targetSize;
	unsigned int c_rxSize;
	unsigned int c_txSize;
	z_stream zStream;
}ClientsOnline;

volatile sig_atomic_t srv_incomingSignal;
void srv_incomingSignal_parse(int signum);
unsigned int processAndSend(int sock, char *data, unsigned int dataLen, void *params);
void showCstat(ClientsOnline* client);
#endif //#ifndef _SERVER_H_
