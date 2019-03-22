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

#include "../protocol/protocol.h"
#include "zlib/zlib.h"
#include "server.h"

void srv_incomingSignal_parse(int signum)
{
	srv_incomingSignal = 1;
}

unsigned int processAndSend(int sock, char *data, unsigned int dataLen, void *params)
{
	unsigned int retval = 0;
	int ret = 0;


	if (dataLen == sizeof(MagicToken))
	{
		if (isHeaderValid(data, dataLen))
		{
			Ack *ack = (Ack*)malloc(sizeof(Ack));
			ack->start_key = MAGIC_START_KEY;
			ack->end_key = MAGIC_END_KEY;
			retval = send(sock, ack, sizeof(Ack), MSG_NOSIGNAL);
			free(ack);
		}
	}
	else
	{
		if (params)
		{
			ClientsOnline *client = (ClientsOnline*) params;
			client->c_rxSize += dataLen;
			switch (client->c_compressionType)
			{
				case noCompression:
					retval =send(sock, data, dataLen, MSG_NOSIGNAL);
					client->c_txSize += retval;
				break;
				case zlibDeflate:
					if (!client->c_txSize)
					{
						if (deflateInit(&client->zStream, ZLIB_DEFLATE_LEVEL) != Z_OK)
						{
							printf("deflate init failed\n");
							//TODO: correct stop
							break;
						}
					}
					client->zStream.next_in = data;
					client->zStream.avail_in = dataLen;
					unsigned char *out = (unsigned char*)malloc(dataLen);
					do
					{
						client->zStream.next_out = out;
						client->zStream.avail_out = dataLen;
						if (deflate(&client->zStream, (client->c_rxSize >= client->c_targetSize)? Z_FINISH : Z_NO_FLUSH) == Z_STREAM_ERROR)
								printf("worker: zlib deflate error %d\n",client->c_txSize);
						else
						{
							retval =send(sock, out, dataLen - client->zStream.avail_out, MSG_NOSIGNAL);
							client->c_txSize += retval;
						}
					}
					while(client->zStream.avail_out == 0);
					free(out);
					if (client->c_rxSize >= client->c_targetSize)
					{
						(void)deflateEnd(&client->zStream);
						printf("worker: zlib def inflateEnd\n");
					}
				break;
				case zlibInflate:
					if (!client->c_txSize)
					{
						if (inflateInit(&client->zStream) != Z_OK)
						{
							printf("inflate init failed\n");
							//TODO: correct stop
							break;
						}
					}
					client->zStream.next_in = data;
					client->zStream.avail_in = dataLen;
					unsigned char *in = (unsigned char*)malloc(dataLen);
					do
					{
						client->zStream.next_out = in;
						client->zStream.avail_out = dataLen;
						ret = inflate(&client->zStream,Z_NO_FLUSH);

						switch (ret)
						{
							case Z_NEED_DICT:
								printf("worker: zlib inflate Z_NEED_DICT\n");
								break;
							case Z_DATA_ERROR:
								printf("worker: zlib inflate Z_DATA_ERROR\n");
								break;
							case Z_MEM_ERROR:
								printf("worker: zlib inflate Z_MEM_ERROR\n");
								break;
							
						}
						if (ret != Z_STREAM_ERROR)
						{
							retval =send(sock, in, dataLen - client->zStream.avail_out, MSG_NOSIGNAL);
							client->c_txSize += retval;
						}
						if (ret == Z_STREAM_END)	
						{
							(void)inflateEnd(&client->zStream);
						printf("worker: zlib inf inflateEnd\n");
						}			
					}
					while(client->zStream.avail_out == 0);
					free(in);
					break;
				default:
					printf("worker: parameter %d\n",client->c_compressionType);
					retval = 0;
				break;
			}
			if (client->c_rxSize >= client->c_targetSize)
			{
				MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
				memset(mToken, 0,sizeof(MagicToken));
				mToken->start_key = MAGIC_START_KEY;
				mToken->compressionType = client->c_compressionType;
				mToken->nextdatasizes = client->c_rxSize;
				mToken->end_key = MAGIC_END_KEY;
				sleep(1);
				send(sock, (char *)mToken, sizeof(MagicToken), MSG_NOSIGNAL);
				free(mToken);
			}
		}
		else
			retval =send(sock, data, dataLen, MSG_NOSIGNAL);
	}//if (dataLen == sizeof(MagicToken))
	return retval;
}
void showCstat(ClientsOnline* client)
{
	printf("\033[7m");
	printf("client %s[%d]: rx/tx %d/%d of %d\n",
								client->c_ip,
								client->c_socket,
								client->c_rxSize,
								client->c_txSize,
								client->c_targetSize);
	printf("\033[0m");
}
