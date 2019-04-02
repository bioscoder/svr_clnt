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
#include "client.h"

const char *Extension[] =
{
	".echo",
	".rdfl",
	".rinfl",
	".zdfl",
	".zinfl",
	".lzcom",
	".lzdec",
	".gz",
	".ugz",
	".err"
};

void clnt_incomingSignal_parse(int signum)
{
	clnt_incomingSignal = 1;
}

int bringUpServer(int socket, unsigned int dataSize)
{
	char * buffer[MAX_BUFFER_SIZE];
	int retv = 0, err_cnt = -1;

	MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
	Ack *ack = (Ack*)malloc(sizeof(Ack));
	if ((!mToken)|| (!ack))
		return 1;
	memset(mToken, 0,sizeof(MagicToken));
	mToken->start_key = MAGIC_START_KEY;
	mToken->compressionType = conf.compressionType;
	mToken->compressionLevel = conf.compressionLevel;
	mToken->chunk_size = conf.chunk_size;
	mToken->nextdatasizes = dataSize;

	mToken->end_key = MAGIC_END_KEY;
	mToken->end_key = MAGIC_END_KEY;

	memset(ack, 0,sizeof(Ack));
	ack->start_key = MAGIC_START_KEY;
	ack->end_key = MAGIC_END_KEY;

	for (;;)
	{
		if (clnt_incomingSignal)
		{
			printf("main_client: catch signal %d. Should exit from thread\n",clnt_incomingSignal);
			break;
		}

		if (err_cnt++ > 100)
			break;

		if (send(socket, mToken, sizeof(MagicToken), 0) != sizeof(MagicToken))
			continue;

		if ((retv = recv(socket, buffer, MAX_BUFFER_SIZE, 0)) <= 0)
		{
			if (retv == 0)
				continue;
			else
			{
				perror("client recv:");
				break;
			}
		}
		else
		{
			if (memcmp(ack, buffer, sizeof(Ack)) != 0)
				continue;
			else
				break;
		}
	}
	if (mToken) free(mToken);
	if (ack) free(ack);

	if (retv == sizeof(Ack))
		return 0;

	return retv;
}

DataStat *txThread(void *params)
{
	ClientThreadsParametes *cp = (ClientThreadsParametes*) params;
	unsigned int tx_data = 0;
	unsigned int idx = 0, t = 0, c_chunk = MAX_BUFFER_SIZE;

	int send_retv, select_retv;
	unsigned int tx_size = 0;
	struct pollfd transmitter[1];
	int attempts = 0;
	int pSocket = cp->c_socket;

	FILE *inFile = fopen(cp->c_filename, "rb");
	unsigned int inFileSize = cp->c_fsize;

	if (inFile == NULL)
	{
		perror("txThread fopen");
		pthread_exit(NULL);
	}
	fseek(inFile , 0 , SEEK_END);
	inFileSize = ftell(inFile);
	rewind(inFile);

	if (cp->c_chunk)
		c_chunk = cp->c_chunk;
	char *send_buffer = (unsigned char*)malloc(c_chunk);
	if (!send_buffer)
	{
		perror("TxThread buffer allocation");
		pthread_exit(NULL);
	}
	struct timeval timeout = {10,0};
	unsigned char timeouts = 4;
	//TODO: signal handling
	while(!clnt_incomingSignal)
	{
		if (clnt_incomingSignal)
		{
			printf("txThread: catch signal %d. Should exit from thread\n",clnt_incomingSignal);
			break;
		}
		fd_set set_sockets_write;
		FD_ZERO(&set_sockets_write);
		FD_SET (pSocket, &set_sockets_write);
		if (tx_data<inFileSize)
		{
			if ((inFileSize - tx_data) < c_chunk)
				(idx = inFileSize - tx_data);
			else
				(idx = c_chunk);
			if ((t = fread(send_buffer, 1, idx,inFile)) != idx)
			{
				if (t == 0)
					continue;
				else
				if (t > 0)
					idx = t;
				else
				{
					perror("txThread read error");
					break;
				}
			}
		}
		else
		{
			FD_CLR(pSocket, &set_sockets_write);
		}
		timeouts--;
		select_retv = select (pSocket + 1, NULL, &set_sockets_write, NULL, &timeout);
		if (select_retv > 0)
		{
			if(FD_ISSET(pSocket, &set_sockets_write))
			{
					attempts = 0;
					do
					{
						send_retv = send(pSocket, send_buffer, idx, 0);

						if (send_retv <= 0)
						{
							perror("txThread send");
							if (attempts++ < 100 )
							{
								if ((errno == EAGAIN))
									attempts = 0;
								else
									break;
							}
						}
					}
					while (send_retv <= 0);
					tx_data += send_retv;
					if (tx_data>=inFileSize)
						break;
			}
		}
		if (select_retv < 0)
		{
			perror("txThread select");
			break;
		}
		if (select_retv = 0)
		{
			if (timeouts > 0)
				continue;
			else
			{
				printf("server timeout. Stop activity\n");
				break;
			}
		}
	}//while(!clnt_incomingSignal)
	if (send_buffer)
		free(send_buffer);
	if(inFile) fclose(inFile);

	gds->tx_bytes = tx_data;
	pthread_exit(gds);
}

DataStat *rxThread(void *params)
{
	ClientThreadsParametes *cp = (ClientThreadsParametes*) params;
	unsigned int out_data = 0;
	unsigned int idx=0, c_chunk = MAX_BUFFER_SIZE;

	int recv_retval, select_retval;
	unsigned int rx_size = 0;
	struct pollfd reciever[1];
	int pSocket = cp->c_socket;

	if (cp->c_chunk)
		c_chunk = cp->c_chunk;
	char *recv_buffer = (unsigned char*)malloc(c_chunk);
	if (!recv_buffer)
	{
		perror("rxThread buffer allocation");
		pthread_exit(NULL);
	}
	FILE *outFile = fopen(strcat((char*)cp->c_filename, Extension[cp->c_compressionType]), "wb");
	if (outFile == NULL)
	{
		perror("rxThread:");
		pthread_exit(NULL);
	}
	struct timeval timeout = {60,0};
	unsigned char timeouts = 4;
	//TODO: signal handling
	while(!clnt_incomingSignal)
	{
		if (clnt_incomingSignal)
		{
			printf("rxThread: catch signal %d. Should exit from thread\n",clnt_incomingSignal);
			break;
		}
		fd_set set_sockets_read;
		FD_ZERO(&set_sockets_read);
		FD_SET (pSocket, &set_sockets_read);
		select_retval = select (pSocket + 1, &set_sockets_read, NULL, NULL, &timeout);
		if (select_retval > 0)
		{
			if(FD_ISSET(pSocket, &set_sockets_read))
			{
				if ((recv_retval = recv(pSocket, recv_buffer, c_chunk, 0)) <= 0)
				{
					if (recv_retval == 0)
						continue;
					else
					{
						perror("rxThread");
						break;
					}
				}
				else
				{
					if (recv_retval == sizeof(MagicToken))
					{
						if (isHeaderValid(recv_buffer, recv_retval))
						{
							MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
							if (mToken == NULL)
							{
								printf("malloc mToken\n");
								return 0;
							}
							if (memmove(mToken, recv_buffer, (sizeof(MagicToken))))
							{
								gds->error = mToken->err_code;
								free(mToken);
							}
							FD_CLR(pSocket, &set_sockets_read);
							break;
						}
					}
					fwrite(recv_buffer , 1, recv_retval, outFile );
					rx_size+=recv_retval;
				}
			}
		}
		if (select_retval < 0)
		{
			perror("rxThread select");
			break;
		}
		if (select_retval = 0)
		{
			if (timeouts--)
				continue;
			else
			{
				printf("server timeout. Stop activity\n");
				break;
			}
		}
	}//while(!clnt_incomingSignal)
	if (recv_buffer)
		free(recv_buffer);
	if(outFile) fclose(outFile);

	gds->rx_bytes = rx_size;
	pthread_exit(gds);
}

DataStat *RxTxThread(void *params)
{
	ClientThreadsParametes *cp = (ClientThreadsParametes*) params;
	unsigned int tx_data = 0;
	unsigned int idx=0, t=0, c_chunk = MAX_BUFFER_SIZE;
	int send_retv, poll_retval;
	unsigned int tx_size = 0;
	struct pollfd pollRxTx[2];
	int attempts = 0;

	unsigned int out_data = 0;
	int recv_retval;
	unsigned int rx_size = 0;
	struct pollfd reciever[1];
	int pSocket = cp->c_socket;

	char* outFilename = (char*)cp->c_filename;
	char* inFilename = (char*)cp->c_filename;

	if (cp->c_chunk)
		c_chunk = cp->c_chunk;
	char *send_buffer = (unsigned char*)malloc(c_chunk);
	char *recv_buffer = (unsigned char*)malloc(c_chunk);
	if ((!recv_buffer) || (!send_buffer))
	{
		perror("RxTxThread buffer allocation");
		pthread_exit(NULL);
	}

	FILE *inFile = fopen(inFilename, "rb");
	if (inFile == NULL)
	{
		perror("RxTxThread input file");
		pthread_exit(NULL);
	}

	FILE *outFile = fopen(strcat(outFilename,  Extension[cp->c_compressionType]), "wb");
	if (outFile == NULL)
	{
		perror("RxTxThread ounput file");
		pthread_exit(NULL);
	}
	unsigned int inFileSize = cp->c_fsize;

	fseek(inFile , 0 , SEEK_END);
	inFileSize = ftell(inFile);
	rewind(inFile);
	sleep(1);
	pollRxTx[0].fd = pSocket;
	pollRxTx[0].events = POLLIN;
	pollRxTx[0].revents = 0;
	pollRxTx[1].fd = pSocket;
	pollRxTx[1].events = POLLOUT;
	pollRxTx[1].revents = 0;

	unsigned char timeouts = 4;
	//TODO: signal handling
	while(!clnt_incomingSignal)
	{
		if (clnt_incomingSignal)
		{
			printf("RxTxThread: catch signal %d. Should exit from thread\n",clnt_incomingSignal);
			break;
		}

		if ((poll_retval = poll(pollRxTx,2, 60*1000)) > 0) //60 seconds timeout
		{
			if (pollRxTx[1].revents & POLLOUT)
			{
				pollRxTx[1].revents &= ~POLLOUT;
				if (tx_data<inFileSize)
				{
					if ((inFileSize - tx_data) < c_chunk)
							(idx = inFileSize - tx_data);
					else
						(idx = c_chunk);

					if ((t = fread(send_buffer, 1, idx,inFile)) != idx)
					{
						if (t == 0)
						{
							continue;
						}
						else
						if (t > 0)
							idx = t;
						else
						{
							perror("RxTxThread read error");
							break;
						}
					}
					attempts = 0;
					do
					{
						send_retv = send(pollRxTx[1].fd, send_buffer, idx, 0);

						if (send_retv <= 0)
						{
							if (attempts++ < 100 )
							{
								if ((errno == EAGAIN))
									attempts = 0;
								else
									break;
							}
						}
					}
					while (send_retv <= 0);
					tx_data += send_retv;
				}
				else
				{
					pollRxTx[1].events &= ~POLLOUT;
				}

			}
			if (pollRxTx[0].revents & POLLIN)
			{
				pollRxTx[0].revents &= ~POLLIN;
				if ((recv_retval = recv(pollRxTx[0].fd, recv_buffer, c_chunk, 0)) <= 0)
				{
					if (recv_retval == 0)
						continue;
					else
					{
						perror("RxTxThread");
						break;
					}
				}
				else
				{
					if (recv_retval == sizeof(MagicToken))
					{
						if (isHeaderValid(recv_buffer, recv_retval))
						{
							MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
							if (mToken == NULL)
							{
								perror("malloc mToken");
								return 0;
							}
							if (memmove(mToken, recv_buffer, (sizeof(MagicToken))))
							{
								gds->error = mToken->err_code;
								free(mToken);
							}
							pollRxTx[0].events &= ~POLLIN;
							break;
						}
					}
					fwrite(recv_buffer , 1, recv_retval, outFile );
					rx_size+=recv_retval;
				}
			}
		}
		else
		if (poll_retval < 0)
		{
			perror("RxTxThread poll");
			break;
		}
		if (poll_retval == 0)
		{
			if (timeouts--)
				continue;
			else
			{
				printf("server timeout. Stop activity\n");
				break;
			}
		}
	}

	if (recv_buffer)
	 	free(recv_buffer);
	if (send_buffer)
		free(send_buffer);

	if(outFile) fclose(outFile);
	if(inFile) fclose(inFile);

	gds->tx_bytes = tx_data;
	gds->rx_bytes = rx_size;

	if (cp->c_model)
		pthread_exit(gds);
	else
		return gds;
}
