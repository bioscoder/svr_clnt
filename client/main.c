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

static void show_help()
{

	printf("Avalible parameters:");
	printf("    [-H  server IP adress\n");
	printf("    [-I  Input file for manupulations\n");
	printf("    [-T  Compression/decomression method\n");
	printf("    [       0 - no compression\n");
	printf("    [       1 - Zlib deflate\n");
	printf("    [       2 - Zlib inflate\n");
	printf("    [       3 - lzma comress\n");
	printf("    [       4 - lzma decomress\n");
	printf("    [-p  Client threading mode.\n");
	printf("    [       0 - Single thread\n");
	printf("    [       1 - Addintional Single thread for rx and tx\n");
	printf("    [       2 - Addintional Separate threads for rx and tx\n");
	printf("    Exapmle: ./hclient -H 127.0.0.1 -I test.bin -T 0\n");
	printf("             ./hclient -H 127.0.0.1 -I test.bin -T 1\n");
	printf("             ./hclient -H 127.0.0.1 -I test.bin.zdfl -T 2\n");
}

int main(int argc, char *argv[])
{
	int opt = 0;
	
	int flags;
	int sock;
	struct sockaddr_in addr;

	sig_atomic_t clnt_incomingSignal = 0;

	signal(SIGTERM, clnt_incomingSignal_parse);

	while ((opt = getopt(argc, argv, "H:I:T:p:h?")) != -1) {
		switch (opt) {
			case 'H':
				conf.host = optarg;
			break;
			case 'I':
				conf.input_filename = optarg;
				FILE * iFile = fopen(conf.input_filename, "rb");
				if (iFile == NULL)
				{
					perror("Input file:");
					exit(1);
				}
				fseek(iFile , 0 , SEEK_END);
				conf.input_fsize = ftell(iFile);
				rewind(iFile);
				fclose(iFile);
				break;
			case 'T':
				conf.arch_type = (unsigned char)atoi(optarg);
				if (conf.arch_type >= unsuppportedCompression)
					conf.arch_type = noCompression;
				break;
			case 'p':
				conf.client_model = (unsigned char)atoi(optarg);
				if ((conf.client_model < 0) && (conf.client_model > 2))
					conf.client_model = 0;
				break;
			case 'h':
			case '?':conf.client_model = (unsigned char)atoi(optarg);
			default:
				show_help();
				exit(0);
			break;
			
		}
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("Socket failed:");
		exit(1);
	}

	opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
	 
	addr.sin_family = AF_INET;
	addr.sin_port = htons(TARGET_PORT);
	addr.sin_addr.s_addr = inet_addr(conf.host);

	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("Connect failed:");
		exit(2);
	}

	if (bringUpServer(sock, conf.input_fsize) == 0)
	{
		printf("Connected to %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		
 		flags = fcntl(sock,F_GETFL,0);
 		if (flags > 0)
		{
			if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0)
				perror("NONBLOCK: ");
		}
		
		ClientThreadsParametes *cp = (ClientThreadsParametes*)malloc(sizeof(ClientThreadsParametes));
		cp->c_compressionType = conf.arch_type;
		cp->c_fsize = conf.input_fsize;
		cp->c_filename = conf.input_filename;
		cp->c_socket = sock;
		cp->c_model = conf.client_model;

		gds = (DataStat*)malloc(sizeof(DataStat));
		time_t start, end;

		if (conf.client_model == 0)
		{
			time(&start);
			gds = RxTxThread((void *)cp);
			time(&end);
		}
		if (conf.client_model == 1)
		{
			pthread_t client_thread;
			
			time(&start);
			pthread_create(&client_thread, NULL, (void *)RxTxThread, (void*)cp);
			pthread_join(client_thread, (void **)&gds);
			time(&end);
		}
		if (conf.client_model == 2)
		{
			pthread_t rx_client_thread;
			pthread_t tx_client_thread;

			time(&start);
			pthread_create(&tx_client_thread, NULL, (void *)txThread, (void*)cp);
			pthread_create(&rx_client_thread, NULL, (void *)rxThread, (void*)cp);

			pthread_join(rx_client_thread, (void **)&gds);
			pthread_join(tx_client_thread, (void **)&gds);
			
			time(&end);		
		}

		OUT_STAT(start, end, gds->tx_bytes, gds->rx_bytes)

		free(gds);
		free(cp);
	}
	close(sock);
	return 0;
}
