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


void clnt_incomingSignal_parse(int signum)
{
	clnt_incomingSignal = 1;
}

void *client_thread(void *params)
{
	ClientThreadsParametes *cp = (ClientThreadsParametes*) params;
	unsigned int out_data = 0;;
	unsigned int idx=0;
	const char *extension;
	char recv_buffer[BUFFER_SIZE] = {0};
	char send_buffer[BUFFER_SIZE] = {0};
	int recv_retval,send_retv, poll_retval;
	unsigned int data_size = 0;
	struct pollfd reciever[2];
	
	switch(cp->c_compressionType)
	{
		case zlibDeflate:
			extension = ".zdfl";
		break;
		case zlibInflate:
			extension = ".zinfl";
		break;
		case noCompression:
			extension = ".echo";
		break;
	}

	FILE *finalf = fopen(strcat((char*)cp->c_filename, extension), "wb");
	if (finalf == NULL)
	{
		perror("recv_client fopen");
		pthread_exit(NULL);
	}

	reciever[0].fd = cp->c_socket;
	reciever[0].events = POLLIN;
	reciever[0].revents = 0;
	reciever[1].fd = cp->c_socket;
	reciever[1].events = POLLOUT;
	reciever[1].revents = 0;
	//TODO: signal handling
	while(clnt_incomingSignal)
	{
		if (clnt_incomingSignal)
		{
			printf("recv_client: catch signal %d. Should exit from thread\n",clnt_incomingSignal);
			break;
		}
		if ((poll_retval = poll(reciever, 2, 10000)) > 0)
		{
			if (reciever[0].revents & POLLIN)
			{
				reciever[0].revents &= ~POLLIN;
				if ((recv_retval = recv(reciever[0].fd, recv_buffer, BUFFER_SIZE, 0)) <= 0)
				{
					if (recv_retval == 0)
						continue;
					else 
					{
						printf("recv_client: err %d\n",errno);
						break;
					}
				}
				else
				{
					if (recv_retval == sizeof(MagicToken))
					{
						if (isHeaderValid(recv_buffer, recv_retval))
						{
							reciever[0].events &= ~POLLIN;
							break;
						}
					}
					fwrite(recv_buffer , 1, recv_retval, finalf );
					data_size+=recv_retval;
				}
			}
			if (reciever[1].revents & POLLOUT)
			{
				reciever[1].revents &= ~POLLOUT;
				if (out_data<cp->c_fsize)
				{
					if ((cp->c_fsize - out_data) < BUFFER_SIZE)
						(idx = cp->c_fsize - out_data);
					else 
						(idx = BUFFER_SIZE);
					
					int attempts = 0;
					if (idx != fread(send_buffer, sizeof(char), idx, cp->c_flink))
					{
						printf("read error\n");	
						fclose(cp->c_flink);
						break;
					}

					do
					{
						send_retv = send(reciever[1].fd, send_buffer, idx, 0);
						if (send_retv <= 0) 
						{
							if (attempts++ < 100 )
							{	printf("recv_client: send retry[%d] erno %d\n", out_data, errno);
								if ((errno == EAGAIN))
									attempts = 0;
								else
									break;
							}
						}
					}
					while (send_retv <= 0);
					out_data += send_retv;
				}
				else
					reciever[1].events &= ~POLLOUT;
			}
		}
		else 
		if (poll_retval <= 0)
		{
			perror("recv_client poll:");
			break;
		}
	}
	if(finalf) fclose(finalf);
	fclose(cp->c_flink);
	pthread_exit(NULL);
}
