#include "tools.h"

void *recv_client(void *params)
{
	ClientThreadsParametes *cp = (ClientThreadsParametes*) params;
	unsigned int out_data = 0;;
	unsigned int idx=0;
	const char *extension;
	switch(cp->c_compressionType)
	{
		case 0:
			extension = ".orig";
		break;
		case 11:
			extension = ".zdfl";
		break;
		//todo.
		default:
			extension = ".arj";
		break;
	}
	
	FILE *finalf = fopen(strcat((char*)cp->c_filename, extension), "wb");
	
	if (finalf == NULL)
	{
		perror("recv_client");
		pthread_exit(NULL);
	}
	struct pollfd reciever[2];
	reciever[0].fd = cp->c_socket;
	reciever[0].events = POLLIN;
	reciever[0].revents = 0;
	
	reciever[1].fd = cp->c_socket;
	reciever[1].events = POLLOUT;
	reciever[1].revents = 0;
	
	
	char recv_buffer[BUFFER_SIZE] = {0};
	char send_buffer[BUFFER_SIZE] = {0};
	int recv_retval,send_retv, poll_retval;
	unsigned int data_size = 0;

	
	while(1)
	{
		if (clnt_incomingSignal){
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
					else {
						printf("recv_client: err %d\n",errno);
//						perror("recv_client recv");
						break;
					}
				}else{
//					printf("recv_client: got %d bytes, need %d\n", recv_retval,recv_retval);
					fwrite(recv_buffer , 1, recv_retval, finalf );
					//sleep(2);
					data_size+=recv_retval;
					if (!cp->c_compressionType)
					{
						if (data_size >= cp->c_fsize){
							reciever[0].events &= ~POLLIN;
							break;
						}
					}
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
						//send_retv = send(reciever[1].fd, &cp->c_flink[out_data], idx, 0);
						send_retv = send(reciever[1].fd, send_buffer, idx, 0);
						if (send_retv <= 0) 
						{
							if (attempts++ < 100 )
							{	printf("recv_client: send retry[%d] erno %d\n", out_data, errno);
								if ((errno == EAGAIN)) attempts = 0;
								else break;
							}
						}
						
					}while (send_retv <= 0);

					out_data += send_retv;
				}else{
					reciever[1].events &= ~POLLOUT;
				}
				
			}
			
		}
		else if (poll_retval <= 0)
		{
			//error
			perror("recv_client got error");
			break;
		}
	}
	if(finalf) fclose(finalf);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int opt = 0;
	unsigned int lSize = 0;
    char * send_buffer = NULL;
	char * recv_buffer = NULL;
	/* char * file_buffer = NULL; */
	time_t start, end;
	int flags;
	int send_retv, recv_retv;
	
	char ack[1] = {'A'};
	
 	signal(SIGINT, clnt_incomingSignal_parse);
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
				  lSize = ftell(conf.outFile);
				  conf.input_fsize = ftell(conf.outFile);
				  rewind (conf.outFile);

				  send_buffer = (char*) malloc(sizeof(char) * 1024);
				  memset(send_buffer, sizeof(char) * lSize, 0);
				  recv_buffer = (char*) malloc(sizeof(char) * 1024);
				  memset(recv_buffer, sizeof(char) * lSize, 0);
				 /*  file_buffer = (char*) malloc(sizeof(char) * lSize);
				  memset(file_buffer, sizeof(char) * lSize, 0); */
				  
				  if ((send_buffer == NULL) || (recv_buffer == NULL)/*|| (file_buffer == NULL)*/)
				  {
					  printf("mem error\n");
					  exit(EXIT_FAILURE);
				  }
				break;
			case 'T':
				conf.arch_type = (unsigned char)atoi(optarg);
				if ((conf.arch_type!= 11) && (conf.arch_type!= 0))
				{
					conf.arch_type =0;
					 printf("Unsupported method. Will use just file echo\n");
				}
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
	
	MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
	mToken->start_key = MAGIC_START_KEY;
	mToken->compressionType = conf.arch_type;
	mToken->nextdatasizes = lSize;
	mToken->end_key = MAGIC_END_KEY;

	
	printf("\n%X %X %d %X\n",
			mToken->start_key,
			mToken->compressionType,
			mToken->nextdatasizes,
			mToken->end_key);
			
	printf("%d bytes of data is ready\n",lSize );
	/*
	if (lSize != fread(file_buffer, sizeof(char), lSize, conf.outFile))
		printf("read error\n");	
	fclose(conf.outFile);
	*/
	
	flags = fcntl(1,F_GETFL,0);
	if (flags > 0)
		if (fcntl(1, F_SETFL, flags | O_NONBLOCK) == 0)
			printf("STDOUT is now unblocked\n");
		
	flags = fcntl(0,F_GETFL,0);
	if (flags > 0)
		if (fcntl(0, F_SETFL, flags | O_NONBLOCK) == 0)
			printf("STDIN is now unblocked\n");

	int sock;
	struct sockaddr_in addr;

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
	memcpy(send_buffer, mToken, sizeof(MagicToken));

	for (;;) {
		
		if (clnt_incomingSignal){
			printf("main_client: catch signal %d. Should exit from thread\n",clnt_incomingSignal);
			break;
		}

		send(sock, send_buffer, sizeof(MagicToken), 0);
		sleep(1);
		if ((flags = recv(sock, recv_buffer, BUFFER_SIZE, 0)) <= 0){
			if (flags == 0)
				continue;
			else {
				perror("recv_client recv");
				printf("recv_client recv: errno %d\n", errno);
				continue;
			}
			sleep(1);
		}else{
			if (memcmp(ack, recv_buffer, 1) != 0)
			{
				printf("BAD ACK!!\r");
				continue;
			} else 
				break;
		}
		
	}
	free(mToken);
	if (flags > 0)
	{
		flags = fcntl(sock,F_GETFL,0);
		if (flags > 0)
			if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0)
				printf("cliens socket is now unblocked\n"); 
			else perror("NONBLOCK: ");

		time(&start);

		pthread_t recv_client_thread;
		ClientThreadsParametes *cp = (ClientThreadsParametes*)malloc(sizeof(ClientThreadsParametes));

		cp->c_wr_pipe = 0;
		cp->c_rd_pipe = 0 ;
		cp->c_compressionType = conf.arch_type;
		cp->c_fsize = conf.input_fsize;
		cp->c_filename = conf.input_filename;
		cp->c_socket = sock;
		//cp->c_flink = file_buffer;
		cp->c_flink = conf.outFile;
		
		pthread_create(&recv_client_thread, NULL, recv_client, (void*)cp);
		
		pthread_join(recv_client_thread, NULL);
		free(cp);
		
		time(&end);
		printf("\n Transfer done in %f sec.!\n",difftime(end, start));
	}
	close(sock);
	free (send_buffer);
	free (recv_buffer);
	/* free(file_buffer); */
	return 0;
}
