#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "../protocol/protocol.h"

#include "client.h"


int main(int argc, char *argv[])
{
	int opt = 0;
	unsigned int lSize = 0;
	char * buffer[BUFFER_SIZE];
	time_t start, end;
	int flags;
	int sock;
	struct sockaddr_in addr;

	sig_atomic_t clnt_incomingSignal = 0;

	signal(SIGTERM, clnt_incomingSignal_parse);

	conf.arch_type = 0;
	conf.outFile = NULL;

	while ((opt = getopt(argc, argv, "H:I:T:h?")) != -1) {
		switch (opt) {
			case 'H':
				conf.host = optarg;
			break;
			case 'I':
				conf.input_filename = optarg;
				conf.outFile = fopen(optarg, "rb");
				if (conf.outFile == NULL)
				{
					printf("ERROR: Input file not found: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				fseek(conf.outFile , 0 , SEEK_END);
				conf.input_fsize = lSize = ftell(conf.outFile);
				rewind (conf.outFile);
				break;
			case 'T':
				conf.arch_type = (unsigned char)atoi(optarg);
				if (conf.arch_type >= unsuppportedCompression)
					conf.arch_type =noCompression;
				break;
			case 'h':
			case '?':
				/*todo*/
				exit(EXIT_SUCCESS);
				break;
			default:
				exit(EXIT_FAILURE);
			
		}
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("socket");
		exit(1);
	}

	opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
	 
	addr.sin_family = AF_INET;
	addr.sin_port = htons(TARGET_PORT);
	addr.sin_addr.s_addr = inet_addr(conf.host);

	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("connect");
		exit(2);
	}
 
	printf("Connected to %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

	//let's go with magic packet
	MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
	memset(mToken, 0,sizeof(MagicToken));
	mToken->start_key = MAGIC_START_KEY;
	mToken->compressionType = (tCompression)conf.arch_type;
	mToken->nextdatasizes = lSize;
	mToken->end_key = MAGIC_END_KEY;

	Ack *ack = (Ack*)malloc(sizeof(Ack));
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

		send(sock, mToken, sizeof(MagicToken), 0);
		sleep(1);
		if ((flags = recv(sock, buffer, BUFFER_SIZE, 0)) <= 0)
		{
			if (flags == 0)
				continue;
			else 
			{
				perror("recv_client recv");
				printf("recv_client recv: errno %d\n", errno);
				continue;
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
	free(ack);
	free(mToken);
	if (flags > 0)
	{
		 flags = fcntl(sock,F_GETFL,0);
		if (flags > 0)
		{
			if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0)
				printf("cliens socket is now unblocked\n"); 
			else perror("NONBLOCK: ");
		} 
		time(&start);
		pthread_t recv_client_thread;
		ClientThreadsParametes *cp = (ClientThreadsParametes*)malloc(sizeof(ClientThreadsParametes));
		cp->c_wr_pipe = 0;
		cp->c_rd_pipe = 0 ;
		cp->c_compressionType = (tCompression)conf.arch_type;
		cp->c_fsize = conf.input_fsize;
		cp->c_filename = conf.input_filename;
		cp->c_socket = sock;
		cp->c_flink = conf.outFile;

		pthread_create(&recv_client_thread, NULL, client_thread, (void*)cp);
		
		pthread_join(recv_client_thread, NULL);
		free(cp);
		
		time(&end);
		printf("\n Transfer done in %f sec.!\n",difftime(end, start));
	}
	close(sock);
	return 0;
}
