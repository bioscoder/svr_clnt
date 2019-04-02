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
#include "custom_zlib/zlib.h"
#include <lzma.h>
#include "server.h"

void srv_incomingSignal_parse(int signum)
{
	srv_incomingSignal = 1;
}
unsigned char getInfoFromHeader(char *input, unsigned int input_size, ClientsOnline* output)
{
	unsigned int retval = 0;

	if (input_size != sizeof(MagicToken))
		return retval;

	MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
	if (mToken == NULL)
	{
		printf("malloc mToken\n");
		return 0;
	}
	if (memmove(mToken, input, (sizeof(MagicToken))))
	{
		output->c_compressionType = mToken->compressionType;
		output->c_compressionLevel = mToken->compressionLevel;
		output->c_targetSize = mToken->nextdatasizes;
		retval = 1;
	}

	free(mToken);
	return retval;
}
unsigned int processAndSend(int sock, char *data, unsigned int dataLen, void *params)
{
	unsigned int retval = 0;
	int zret = 0;
	int gzret = 0;
	lzma_ret lret = LZMA_OK;
	lzma_action lAction = 0;
	unsigned int some_cnt = 0;

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
					retval = send(sock, data, dataLen, MSG_NOSIGNAL);
					client->c_txSize += retval;
				break;//noCompression

				case rawDeflate:
					zret = Z_OK;
					if (!client->c_txSize)
					{
						client->rStream.zalloc = Z_NULL;
						client->rStream.zfree = Z_NULL;
						client->rStream.opaque = Z_NULL;
						client->rStream.next_in = Z_NULL;
						zret = deflateInit2(&client->rStream, client->c_compressionLevel, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
					}
					if (zret != Z_OK)
					{
						client->c_error = zret;
						printf("worker: raw deflate initialization error: %d\n", zret);
						break;
					}
					client->rStream.next_in = data;
					client->rStream.avail_in = dataLen;
					unsigned char *r_out = (unsigned char*)malloc(dataLen*2);
					do
					{
						client->rStream.next_out = r_out;
						client->rStream.avail_out = dataLen*2;
						zret = deflate(&client->rStream, (client->c_rxSize >= client->c_targetSize)? Z_FINISH : Z_NO_FLUSH);
						if (( zret != Z_OK) && (zret != Z_STREAM_END))
						{
							client->c_error = zret;
							printf("worker: raw deflate error: %d\n",zret);
							break;
						}
							retval = send(sock, r_out, dataLen*2 - client->rStream.avail_out, MSG_NOSIGNAL);
							client->c_txSize += retval;
					}
					while(client->rStream.avail_out == 0);
					free(r_out);
					if ((client->c_rxSize >= client->c_targetSize) || zret)
						(void)deflateEnd(&client->rStream);
				break;//rawDeflate

				case rawInflate:
					zret = Z_OK;
					if (!client->c_txSize)
					{
						client->zStream.zalloc = Z_NULL;
						client->zStream.zfree = Z_NULL;
						client->zStream.opaque = Z_NULL;
						client->zStream.avail_in = 0;
						client->zStream.next_in = Z_NULL;
						zret = inflateInit2(&client->zStream, RAW_DEFLATE_WINDOW);
					}
					if (zret != Z_OK)
					{
						client->c_error = zret;
						printf("worker: zlib inflate initialization error: %d\n", zret);
						break;
					}
					client->zStream.next_in = data;
					client->zStream.avail_in = dataLen;
					unsigned char *r_in = (unsigned char*)malloc(dataLen*2);
					do
					{
						client->zStream.next_out = r_in;
						client->zStream.avail_out = dataLen*2;
						zret = inflate(&client->zStream,Z_NO_FLUSH);
						if (( zret != Z_OK) && (zret != Z_STREAM_END))
						{
							client->c_error = zret;
							printf("worker: zlib inflate error: %d\n",zret);
							break;
						}
						retval = send(sock, r_in, dataLen*2 - client->zStream.avail_out, MSG_NOSIGNAL);
						client->c_txSize += retval;
					}
					while(client->zStream.avail_out == 0);
					free(r_in);
					if ((zret == Z_STREAM_END) || zret)
							(void)inflateEnd(&client->zStream);
				break;//rawInflate

				case zlibDeflate:
					zret = Z_OK;
					if (!client->c_txSize)
					{
						client->zStream.zalloc = Z_NULL;
						client->zStream.zfree = Z_NULL;
						client->zStream.opaque = Z_NULL;
						client->zStream.avail_out = 0;
						//zret = deflateInit2(&client->zStream, client->c_compressionLevel, Z_DEFLATED, ZLIB_DEFLATE_WINDOW, 8, Z_DEFAULT_STRATEGY);
						zret = deflateInit(&client->zStream, client->c_compressionLevel);
					}
					if (zret != Z_OK)
					{
						client->c_error = zret;
						printf("worker: zlib deflate initialization error: %d\n", zret);
						break;
					}
					client->zStream.next_in = data;
					client->zStream.avail_in = dataLen;
					unsigned char *z_out = (unsigned char*)malloc(dataLen);
					do
					{
						client->zStream.next_out = z_out;
						client->zStream.avail_out = dataLen;
						zret = deflate(&client->zStream, (client->c_rxSize >= client->c_targetSize)? Z_FINISH : Z_NO_FLUSH);
						if (( zret != Z_OK) && (zret != Z_STREAM_END))
						{
							client->c_error = zret;
							printf("worker: zlib deflate error: %d\n",zret);
							break;
						}
						retval = send(sock, z_out, dataLen - client->zStream.avail_out, MSG_NOSIGNAL);
						client->c_txSize += retval;
					}
					while(client->zStream.avail_out == 0);
					free(z_out);
					if ((client->c_rxSize >= client->c_targetSize) || zret)
						(void)deflateEnd(&client->zStream);
				 break;//zlibDeflate

				case zlibInflate:
					zret = Z_OK;
					if (!client->c_txSize)
					{
						client->zStream.zalloc = Z_NULL;
						client->zStream.zfree = Z_NULL;
						client->zStream.opaque = Z_NULL;
						client->zStream.avail_in = 0;
						client->zStream.next_in = Z_NULL;
						//zret = inflateInit2(&client->zStream, ZLIB_DEFLATE_WINDOW);
						zret = inflateInit(&client->zStream);
					}
					if (zret != Z_OK)
					{
						client->c_error = zret;
						printf("worker: zlib inflate initialization error: %d\n", zret);
						break;
					}
					client->zStream.next_in = data;
					client->zStream.avail_in = dataLen;
					unsigned char *z_in = (unsigned char*)malloc(dataLen);
					do
					{
						client->zStream.next_out = z_in;
						client->zStream.avail_out = dataLen;
						zret = inflate(&client->zStream,Z_NO_FLUSH);
						if (( zret != Z_OK) && (zret != Z_STREAM_END))
						{
							client->c_error = zret;
							printf("worker: zlib inflate error: %d\n",zret);
							break;
						}
						retval = send(sock, z_in, dataLen - client->zStream.avail_out, MSG_NOSIGNAL);
						client->c_txSize += retval;
					}
					while(client->zStream.avail_out == 0);
					free(z_in);
					if ((zret == Z_STREAM_END) || zret)
							(void)inflateEnd(&client->zStream);
				break;//zlibInflate

				case lzmaCompress:
					lret = LZMA_OK;
					if (!client->c_txSize)
					{
						client->lzStream = (lzma_stream)LZMA_STREAM_INIT;
						lret = lzma_easy_encoder(&client->lzStream, client->c_compressionLevel | LZMA_PRESET_EXTREME, LZMA_CHECK_CRC64);
					}
					if (lret != LZMA_OK)
					{
						client->c_error = lret;
						printf("worker: lzma compress initialization error: %d\n", lret);
						break;
					}
					client->lzStream.next_in = data;
					client->lzStream.avail_in = dataLen;
					unsigned char *l_out = (unsigned char*)malloc(dataLen);
					lAction = (client->c_rxSize >= client->c_targetSize) ? LZMA_FINISH : LZMA_RUN;
					do
					{
						client->lzStream.next_out = l_out;
						client->lzStream.avail_out = dataLen;

						lret = lzma_code(&client->lzStream, lAction);

						if ((lret != LZMA_OK) && (lret != LZMA_STREAM_END))
						{
							client->c_error = lret;
							printf("worker: lzma compression error %d [%d]\n",lret,client->c_txSize);
							break;
						}
						retval = send(sock, l_out, dataLen - client->lzStream.avail_out, MSG_NOSIGNAL);
						client->c_txSize += retval;
					}
					while(client->lzStream.avail_out == 0);
					if ((client->c_rxSize >= client->c_targetSize) || lret)
						lzma_end(&client->lzStream);
					free(l_out);
				break;//lzmaCompress

				case lzmaDeCompress:
					lret = LZMA_OK;
					if (!client->c_txSize)
					{
						client->lzStream = (lzma_stream)LZMA_STREAM_INIT;
						lret = lzma_stream_decoder(&client->lzStream, UINT64_MAX, LZMA_CONCATENATED);
					}
					if (lret != LZMA_OK)
					{
						client->c_error = lret;
						printf("worker: lzma decompress initialization error: %d\n", lret);
						break;
					}
					client->lzStream.next_in = data;
					client->lzStream.avail_in = dataLen;
					unsigned char *l_in = (unsigned char*)malloc(dataLen);
					lAction = (client->c_rxSize >= client->c_targetSize) ? LZMA_FINISH : LZMA_RUN;
					do
					{
						client->lzStream.next_out = l_in;
						client->lzStream.avail_out = dataLen;
						lret = lzma_code(&client->lzStream, lAction);

						if ((lret != LZMA_OK) && (lret != LZMA_STREAM_END))
						{
							client->c_error = lret;
							printf("worker: lzma decompression error %d [%d]\n",lret,client->c_txSize);
							break;
						}
						retval = send(sock, l_in, dataLen - client->lzStream.avail_out, MSG_NOSIGNAL);
						client->c_txSize += retval;
					}
					while(client->lzStream.avail_out == 0);
					if ((client->c_rxSize >= client->c_targetSize) || lret)
						lzma_end(&client->lzStream);
					free(l_in);
				break;//lzmaDeCompress

				case gzCompress:
					gzret = Z_OK;
					if (!client->c_txSize)
					{
						client->gzStream.zalloc = Z_NULL;
						client->gzStream.zfree = Z_NULL;
						client->gzStream.opaque = Z_NULL;
						client->gzStream.avail_in = 0;
						client->gzStream.next_in = Z_NULL;
						gzret = deflateInit2(&client->gzStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, GZIP_WINDOW, 8, Z_DEFAULT_STRATEGY);
					}
					if (zret != Z_OK)
					{
						client->c_error = gzret;
						printf("worker: zlib deflate initialization error: %d\n", gzret);
						break;
					}
					client->gzStream.next_in = data;
					client->gzStream.avail_in = dataLen;
					unsigned char *gz_out = (unsigned char*)malloc(dataLen);
					do
					{
						client->gzStream.next_out = gz_out;
						client->gzStream.avail_out = dataLen;
						gzret = deflate(&client->gzStream, (client->c_rxSize >= client->c_targetSize)? Z_FINISH : Z_NO_FLUSH);
						if (( gzret != Z_OK) && (gzret != Z_STREAM_END))
						{
							client->c_error = gzret;
							printf("worker: zlib deflate error: %d\n",gzret);
							break;
						}
						retval = send(sock, gz_out, dataLen - client->gzStream.avail_out, MSG_NOSIGNAL);
						client->c_txSize += retval;
					}
					while(client->gzStream.avail_out == 0);
					free(gz_out);
					if ((client->c_rxSize >= client->c_targetSize) || gzret)
						(void)deflateEnd(&client->gzStream);
				break;//gzCompress

				case gzDeCompress:
					gzret = Z_OK;
					if (!client->c_txSize)
					{
						client->gzStream.zalloc = Z_NULL;
						client->gzStream.zfree = Z_NULL;
						client->gzStream.opaque = Z_NULL;
						client->gzStream.avail_in = 0;
						client->gzStream.next_in = Z_NULL;
						gzret = inflateInit2(&client->gzStream, GZIP_WINDOW);
					}
					if (gzret != Z_OK)
					{
						client->c_error = gzret;
						printf("worker: zlib inflate initialization error: %d\n", gzret);
						break;
					}
					client->gzStream.next_in = data;
					client->gzStream.avail_in = dataLen;
					unsigned char *gz_in = (unsigned char*)malloc(dataLen*2);
					do
					{
						client->gzStream.next_out = gz_in;
						client->gzStream.avail_out = dataLen*2;
						gzret = inflate(&client->gzStream,Z_NO_FLUSH);

						if (( gzret != Z_OK) && (gzret != Z_STREAM_END)/* && (gzret != Z_BUF_ERROR)*/)
						{
							client->c_error = gzret;
							printf("worker: zlib inflate error: %d \n",gzret);
							break;
						}
						retval = send(sock, gz_in, dataLen*2 - client->gzStream.avail_out, MSG_NOSIGNAL);
						client->c_txSize += retval;
					}
					while(client->gzStream.avail_out == 0);
					free(gz_in);
					if ((gzret == Z_STREAM_END) || gzret)
						(void)inflateEnd(&client->gzStream);
				break;//gzDeCompress
				default:
					printf("worker: parameter %d\n",client->c_compressionType);
					retval = 0;
				break;
			}
			if ((client->c_rxSize >= client->c_targetSize) || (client->c_error))
			{
				MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
				memset(mToken, 0,sizeof(MagicToken));
				mToken->start_key = MAGIC_START_KEY;
				mToken->compressionType = client->c_compressionType;
				mToken->nextdatasizes = client->c_rxSize;
				mToken->err_code = client->c_error;
				mToken->end_key = MAGIC_END_KEY;
				sleep(1);
				send(sock, (char *)mToken, sizeof(MagicToken), MSG_NOSIGNAL);
				free(mToken);
			}
		}
		else
			retval = send(sock, data, dataLen, MSG_NOSIGNAL);
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
