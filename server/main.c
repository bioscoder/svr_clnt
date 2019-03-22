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

int main()
{
	fd_set main_set_fd;
	fd_set read_set_fd;
	int max_seen_socket;
	int new_socket;
	struct sockaddr_storage clientaddr;
	socklen_t socklen;
	int listener;
	char incoming_data[BUFFER_SIZE];
	int sock;
	char remoteIP[INET_ADDRSTRLEN];
	struct sockaddr_in addr;
	int retv_recv;
	ClientsOnline cOnline[MAX_ONLINE_CLIENTS];
	sig_atomic_t srv_incomingSignal = 0;
	
	signal(SIGTERM, srv_incomingSignal_parse);
	signal(SIGINT, srv_incomingSignal_parse);

	
	listener = socket(AF_INET, SOCK_STREAM, 0);
	if(listener < 0)
	{
		perror("socket");
		exit(1);
	}

	sock = 1;
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &sock, sizeof(int));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(TARGET_PORT);
	addr.sin_addr.s_addr = htonl(CLIENTS_IFACE_ADDR);
	if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind failed");
		close(listener);
		exit(2);
	}
	listen(listener, 25);
	FD_ZERO(&main_set_fd);
	FD_ZERO(&read_set_fd);
	FD_SET(listener, &main_set_fd);

	max_seen_socket = listener;
	printf("selectserver: waiting for connection on %d socket\n", listener);
	volatile int cIdx = -1;
	int i = 0;
	memset(cOnline, 0, MAX_ONLINE_CLIENTS*sizeof(ClientsOnline));
	while (!srv_incomingSignal)
	{
		read_set_fd = main_set_fd;
		if (select(max_seen_socket+1, &read_set_fd, NULL, NULL, NULL) == -1)
		{
			perror("select");
			exit(4);
		}
		if (srv_incomingSignal)
		{
			printf("catch signal. Should exit\n");
			//TODO: clean up on emergency exit.
			break;
		}
		for(int current_socket = 0; current_socket <= max_seen_socket; current_socket++)
		{
			if (FD_ISSET(current_socket, &read_set_fd))
			{
				if (current_socket == listener)
				{
					if (cIdx < MAX_ONLINE_CLIENTS)
					{
						socklen = sizeof clientaddr;
						new_socket = accept(listener,(struct sockaddr *)&clientaddr, &socklen);
						if (new_socket == -1)
							perror("accept");
						else
						{
							FD_SET(new_socket, &main_set_fd);
							if (new_socket > max_seen_socket)
								max_seen_socket = new_socket;
							
							if (cIdx < 0)
								cIdx++;
							else
							{
								for (i = 0; i < cIdx; i++) //find free slot, and re-use it
								{
									if (!cOnline[i].inUse)
										cIdx = i;
								}
								if (i == cIdx) //no empty slots
									cIdx++;
							} 
							cOnline[cIdx].c_socket = new_socket;
							cOnline[cIdx].inUse = 0;
							cOnline[cIdx].c_addr = &clientaddr;

							inet_ntop(clientaddr.ss_family,&(((struct sockaddr_in*)((struct sockaddr*)&clientaddr))->sin_addr),remoteIP, INET_ADDRSTRLEN );
							
							memcpy(&cOnline[cIdx].c_ip,&remoteIP,INET_ADDRSTRLEN);

							i = 1;
							setsockopt(cOnline[cIdx].c_socket, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int));
							
							i = fcntl(cOnline[cIdx].c_socket,F_GETFL,0);
							if (i > 0)
							{
								if (fcntl(cOnline[cIdx].c_socket, F_SETFL, i | O_NONBLOCK) != 0)
									perror("NONBLOCK: ");
							}
							printf("client %d[%d] from %s\n",cIdx, cOnline[cIdx].c_socket, cOnline[cIdx].c_ip);
						}
					}
				}
				else
				{
					if ((retv_recv = recv(current_socket, incoming_data, BUFFER_SIZE, 0)) <= 0)
					{
						if (retv_recv == 0)
							printf("selectserver: ret socket %d", current_socket);
						else
							perror("recv@current_socket:");

						close(current_socket);
						FD_CLR(current_socket, &main_set_fd);
					}
					else
					{
						for (i = 0; i < cIdx; i++)
						{
							if ((cOnline[i].c_socket == current_socket))
								break;
						}
						if (!cOnline[i].inUse)
						{
							cOnline[i].inUse = 1;
								cOnline[i].c_targetSize = getFilesizeFromHeader(incoming_data);
								cOnline[i].c_compressionType = getCompressionFromHeader(incoming_data);
								printf("client %d[%d]: target: %d, compr:%d\n",
										i, cOnline[i].c_socket, cOnline[i].c_targetSize, cOnline[i].c_compressionType);

						}

						processAndSend(current_socket, incoming_data, retv_recv, &cOnline[i]);

						if (cOnline[i].c_rxSize == cOnline[i].c_targetSize)
						{
							showCstat(&cOnline[i]);
							FD_CLR(cOnline[i].c_socket, &main_set_fd);
							close(cOnline[i].c_socket);
							memset(&cOnline[i], 0, sizeof(ClientsOnline));
						}
					}
				}
			}
		}
	}
	close(listener);
	return 0;
}
