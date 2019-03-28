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
#include <zlib.h>
#include <lzma.h>
#include "server.h"

void srv_incomingSignal_parse(int signum)
{
	srv_incomingSignal = 1;
}

unsigned int processAndSend(int sock, char *data, unsigned int dataLen, void *params)
{
	unsigned int retval = 0;
	int zret = 0;
	lzma_ret lret = LZMA_OK;

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
					zret = Z_OK;
					if (!client->c_txSize)
						zret = deflateInit(&client->zStream, ZLIB_DEFLATE_LEVEL);
					if (zret != Z_OK)
					{
						printf("worker: zlib deflate initialization error: %d\n", zret);
						break;
					}
					client->zStream.next_in = data;
					client->zStream.avail_in = dataLen;
					unsigned char *z_out = (unsigned char*)malloc(dataLen);
					memset(z_out, 0,sizeof(z_out));
					do
					{
						client->zStream.next_out = z_out;
						client->zStream.avail_out = dataLen;
						zret = deflate(&client->zStream, (client->c_rxSize >= client->c_targetSize)? Z_FINISH : Z_NO_FLUSH);
						if (( zret != Z_OK) && (zret != Z_STREAM_END))
						{
							printf("worker: zlib deflate error: %d\n",zret);
							break;
						}
						//if ((client->zStream.avail_out != dataLen)) 
						{
							retval =send(sock, z_out, dataLen - client->zStream.avail_out, MSG_NOSIGNAL);
							client->c_txSize += retval;
						}
					}
					while(client->zStream.avail_out == 0);
					free(z_out);
					if ((client->c_rxSize >= client->c_targetSize) || zret)
						(void)deflateEnd(&client->zStream);
				break;
				case zlibInflate:
					zret = Z_OK;
					if (!client->c_txSize)
						zret = inflateInit(&client->zStream);
					if (zret != Z_OK)
					{
						printf("worker: zlib inflate initialization error: %d\n", zret);
						break;
					}
					client->zStream.next_in = data;
					client->zStream.avail_in = dataLen;
					unsigned char *z_in = (unsigned char*)malloc(dataLen);
					memset(z_in, 0,sizeof(z_in));
					do
					{
						client->zStream.next_out = z_in;
						client->zStream.avail_out = dataLen;
						zret = inflate(&client->zStream,Z_NO_FLUSH);
						if (( zret != Z_OK) && (zret != Z_STREAM_END))
						{
							printf("worker: zlib inflate error: %d\n",zret);
							break;
						}
						retval =send(sock, z_in, dataLen - client->zStream.avail_out, MSG_NOSIGNAL);
						client->c_txSize += retval;
					}
					while(client->zStream.avail_out == 0);
					free(z_in);
					if ((zret == Z_STREAM_END) || zret)
							(void)inflateEnd(&client->zStream);
					break;
				case lzmaCompress:
					lret = LZMA_OK;
					if (!client->c_txSize)
					{
						client->lzStream = (lzma_stream)LZMA_STREAM_INIT;
						lret = lzma_easy_encoder(&client->lzStream, 6 | LZMA_PRESET_EXTREME, LZMA_CHECK_CRC64);
					}
					if (lret != LZMA_OK) 
					{
						printf("worker: lzma compress initialization error: %d\n", lret);
						break;
					}
					client->lzStream.next_in = data;
					client->lzStream.avail_in = dataLen;
					unsigned char *l_out = (unsigned char*)malloc(dataLen);
					memset(l_out, 0,sizeof(l_out));
					do
					{
						client->lzStream.next_out = l_out;
						client->lzStream.avail_out = dataLen;
						
						lret = lzma_code(&client->lzStream, (client->c_rxSize >= client->c_targetSize) ? LZMA_FINISH : LZMA_RUN);

						if ((lret != LZMA_OK) && (lret != LZMA_STREAM_END))
						{
							printf("worker: lzma compression error %d [%d]\n",lret,client->c_txSize);
							break;
						}
						//if ((client->lzStream.avail_out != dataLen)) 
						{
							retval =send(sock, l_out, dataLen - client->lzStream.avail_out, MSG_NOSIGNAL);
							client->c_txSize += retval;
						}
					}
					while(client->lzStream.avail_out == 0);
					
					if ((client->c_rxSize >= client->c_targetSize) || lret)
					{
						lzma_end(&client->lzStream);
						printf("worker: lzma compression lzma_end\n");
					}
					free(l_out);
				break;
				case lzmaDeCompress:
					lret = LZMA_OK;
					if (!client->c_txSize)
					{
						client->lzStream = (lzma_stream)LZMA_STREAM_INIT;
						lret = lzma_stream_decoder(&client->lzStream, UINT64_MAX, LZMA_CONCATENATED);
					}
					if (lret != LZMA_OK) 
						break;

					client->lzStream.next_in = data;
					client->lzStream.avail_in = dataLen;
					unsigned char *l_in = (unsigned char*)malloc(dataLen);
					memset(l_in, 0,sizeof(l_in));
					do
					{
						client->lzStream.next_out = l_in;
						client->lzStream.avail_out = dataLen;
						
						lret = lzma_code(&client->lzStream, (client->c_rxSize >= client->c_targetSize) ? LZMA_FINISH : LZMA_RUN);

						if ((lret != LZMA_OK) && (lret != LZMA_STREAM_END))
						{
							printf("worker: lzma decompression error %d [%d]\n",lret,client->c_txSize);
							break;//!!!
						}
						//if ((client->lzStream.avail_out != dataLen)) 
						{
							retval =send(sock, l_in, dataLen - client->lzStream.avail_out, MSG_NOSIGNAL);
							client->c_txSize += retval;
						}
					}
					while(client->lzStream.avail_out == 0);
					free(l_in);
					if ((client->c_rxSize >= client->c_targetSize) || lret)
						lzma_end(&client->lzStream);
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
