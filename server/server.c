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

#include "server.h"

void srv_incomingSignal_parse(int signum)
{
	srv_incomingSignal = 1;
}

unsigned int processAndSend(int sock, char *data, unsigned int dataLen, void *params)
{
	unsigned int retval = 0;

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

			switch (client->c_compressionType)
			{
				case noCompression:
						client->c_rxSize += dataLen;
						retval =send(sock, data, dataLen, MSG_NOSIGNAL);
						client->c_txSize += retval;
					break;
				default:
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
	printf("selectserver: client %s[%d]: rx/tx %d/%d of %d\n",
								client->c_ip,
								client->c_socket,
								client->c_rxSize,
								client->c_txSize,
								client->c_targetSize);
}