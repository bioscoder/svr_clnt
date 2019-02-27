#include "tools.h"


void *worker(void *params)
{
	WorkerParameters *wp = (WorkerParameters*) params;
	
	char in_buf[BUFFER_SIZE] = {0};
	char ack[1] = {'A'};
	int bytes_read = 0, total_in = 0, total_out = 0;
	long dataCnt = 0;
	int pipe_fd[2];
	char pipe_buf[BUFFER_SIZE] = {0};
	int send_retv = 0, recv_retv = 0, select_retv = 0;

	if (pipe(pipe_fd)<0)
	{
		printf("pipe failed");
	}

	send_retv = fcntl(pipe_fd[0],F_GETFL,0);
		if (send_retv > 0) 
			if (fcntl(pipe_fd[0], F_SETFL, send_retv | O_NONBLOCK) != 0)
			{
				perror("pipe[0] set N");
			}
	
	send_retv = fcntl(pipe_fd[1],F_GETFL,0);
		if (send_retv > 0) 
			if (fcntl(pipe_fd[1], F_SETFL, send_retv | O_NONBLOCK) != 0)
			{
				perror("pipe[1] set N");
			}
	total_in = total_out = 0;
	send(wp->w_socket, ack, 1, 0);

	struct pollfd poll_worker[2];
	poll_worker[0].fd = wp->w_socket;
	poll_worker[0].events = POLLIN;
	poll_worker[1].fd = pipe_fd[0];
	poll_worker[1].events = POLLIN;
#if 0
	int z_ret = 0;
	unsigned char z_in[Z_CHUNK];
	unsigned char z_out[Z_CHUNK];
	z_stream strm;
	
	if (wp->w_compressionType == 11)
	{
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		z_ret = deflateInit(&strm, Z_NO_COMPRESSION);
		if (z_ret != Z_OK)
		{
			perror("zlib init failed");
		} 
	}
#endif
	int chRd = 0;
	int chdataCnt = 0;
	for (;;){
		if (srv_incomingSignal){
			printf("worker: catch signal %d. Should exit from thread\n",srv_incomingSignal);
			break;
		}
		if ((select_retv = poll(poll_worker, 2, 100)) > 0)
		{
			if (poll_worker[0].revents & POLLIN) {
				poll_worker[0].revents &= ~POLLIN;
				if ((recv_retv = recv(wp->w_socket, in_buf, BUFFER_SIZE, 0)) <= 0) {
						if (recv_retv == 0)
						{
							continue;
						} else
						{
							perror("worker fatal error");
							break;
						}
				} else {
					if (wp->w_compressionType == 0)
					{
						if ((recv_retv = write(pipe_fd[1], in_buf,recv_retv)) <= 0)
						{
							perror("worker: pipe w+\n");
						}
						
						total_in += recv_retv;
						
						//printf("WRK[%d] get %ld by %d of %ld \n",wp->w_socket, total_in, recv_retv, wp->fsize);
						
						if (total_in >= wp->fsize)
							poll_worker[0].events &= ~POLLIN;
					} else
						#if 0
					if (wp->w_compressionType == 11)
					{
						printf("incoming datasize: %d\n", recv_retv);
						total_in += recv_retv;
						
						memcpy(z_in, in_buf, recv_retv);
						strm.avail_in = Z_CHUNK;
						strm.next_in = z_in;
						do {
							strm.avail_out = Z_CHUNK;
							strm.next_out = z_out;
							
							z_ret = deflate(&strm, (total_in >= wp->fsize)? Z_FINISH : Z_NO_FLUSH);
							if (z_ret != Z_OK)
								printf("worker: zlib deflate %d\n", z_ret);
							
							if (((Z_CHUNK - strm.avail_out) != write(pipe_fd[1], z_out,(Z_CHUNK - strm.avail_out)))){
								perror("worker: pipe w+\n");
								 (void)deflateEnd(&strm);
							}
						}while(strm.avail_out == 0);
						
						printf("worker: in %ld bytes by %d\n", total_in,recv_retv);
						if (total_in >= wp->fsize){
							deflateEnd(&strm);
							poll_worker[0].events &= ~POLLIN;
						}
					} else
						#endif
					{
						printf("Unsupported compression/decompression type!\n");
						break;
					}
//						for(int r=0; r < recv_retv; r++)
//							printf("%c", in_buf[r]);
//						printf("\n");
				}
			}
			if (poll_worker[1].revents & POLLIN) {
				poll_worker[1].revents &= ~POLLIN;
					//while (chdataCnt < wp->fsize)
					{
					if ((wp->fsize - total_out) < BUFFER_SIZE)
						chRd = (wp->fsize - total_out);
					else
						chRd = BUFFER_SIZE;
					memset(pipe_buf, 0,chRd);
					bytes_read = read(pipe_fd[0], pipe_buf, chRd);
					if (bytes_read > 0){
						send_retv = send(wp->w_socket, pipe_buf, bytes_read, 0);
						total_out += send_retv;
						//printf("WRK[%d] out %ld by %d of %ld \n",wp->w_socket, total_out, send_retv, wp->fsize);
						if (bytes_read < BUFFER_SIZE){
							poll_worker[1].events &= ~POLLIN;
							break;
						}
					}else if (bytes_read == 0){
						printf("pipe EOF %d\n", chdataCnt);
						break;
					}
				}
			}
		}
	}//for (;;)

	close(pipe_fd[0]);
	close(pipe_fd[1]);

	close(wp->w_socket);
	printf("WRK[%d]: get %ld of %ld bytes. lost: %ld\n",
					wp->w_socket, total_in,wp->fsize,wp->fsize - total_out);
	
	free(wp);
	pthread_exit(NULL);

}
int main()
{
	fd_set main_set_fd;
	fd_set read_set_fd;
	fd_set write_set_fd;
	int max_fd;
	int new_connection;
	struct sockaddr_storage clientaddr;
	socklen_t socklen;
	
	signal(SIGINT, srv_incomingSignal_parse);
	signal(SIGTERM, srv_incomingSignal_parse);
	
	int listener;
	char buf2[sizeof(MagicToken)];

	int sock;
	char remoteIP[INET_ADDRSTRLEN ];
	struct sockaddr_in addr;
	int retv_recv;
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
	if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		perror("bind failed");
		close(listener);
		exit(2);
	}
	int flags = fcntl(listener,F_GETFL,0);
	if (flags > 0)
		if (fcntl(listener, F_SETFL, flags | O_NONBLOCK) > 0)
			printf("server socket is now unblocked\n");
		
	listen(listener, 25);
	FD_ZERO(&main_set_fd);
	FD_ZERO(&read_set_fd);
	
	FD_SET(listener, &main_set_fd);
	
	max_fd = listener;
	 printf("selectserver: waiting for connection \n");
	

	for(;;){
		if (srv_incomingSignal){
			printf("selectserver: catch signal %d. Should exit from thread\n",srv_incomingSignal);
			break;
		}
		read_set_fd = main_set_fd;
		if (select(max_fd+1, &read_set_fd, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}
		for(int i = 0; i <= max_fd; i++) {
			if (FD_ISSET(i, &read_set_fd)) {
				if (i == listener) {
					socklen = sizeof clientaddr;
					new_connection = accept(listener,(struct sockaddr *)&clientaddr,&socklen);
					if (new_connection == -1) {
						perror("accept");
					} else {
						FD_SET(new_connection, &main_set_fd);
						if (new_connection > max_fd) {
							max_fd = new_connection;
						}
						printf("selectserver: new clien from %s on socket %d\n",
							inet_ntop(clientaddr.ss_family,&(((struct sockaddr_in*)((struct sockaddr*)&clientaddr))->sin_addr),
								remoteIP, INET_ADDRSTRLEN ),
							new_connection);
					}
				} else {

					 memset(buf2, sizeof(MagicToken), 0); 
					 
					if ((retv_recv = recv(i, buf2, sizeof(MagicToken), 0)) <= 0) {
						if (retv_recv == 0) {
							printf("selectserver: socket %d hung upn", i);
						} else {
							perror("recv");
						}
						close(i);
						FD_CLR(i, &main_set_fd);
					} else {

						if (retv_recv == sizeof(MagicToken)){
							MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
							
							if (mToken == NULL){
								printf("malloc mToken\n");
								continue;
							}
							mToken->start_key = (buf2[0] & 0xff) | 
												((buf2[1] & 0xff) << 8) | 
												((buf2[2] & 0xff) << 16) | 
												((buf2[3] & 0xff) << 24);
							mToken->compressionType = buf2[4];
							mToken->nextdatasizes = (buf2[5] & 0xff) | 
													((buf2[6] & 0xff)  << 8) | 
													((buf2[7] & 0xff)  << 16) | 
													((buf2[8] & 0xff)  << 24) | 
													((buf2[9] & 0xff)  << 32) |
													((buf2[10] & 0xff) << 40) |
													((buf2[11] & 0xff) << 48) |
													((buf2[12] & 0xff) << 54);
							mToken->end_key = (buf2[13] & 0xff) | 
												((buf2[14] & 0xff) << 8) | 
												((buf2[15] & 0xff) << 16) | 
												((buf2[16] & 0xff) << 24);
							
							if ((mToken->start_key == MAGIC_START_KEY) && (mToken->end_key == MAGIC_END_KEY))
							{
								WorkerParameters *wp = (WorkerParameters*)malloc(sizeof(WorkerParameters));
								if (wp == NULL){
									perror("WP allocate error!\n");
									exit(3);
								}
								wp->w_socket = i;
								wp->fsize = mToken->nextdatasizes;
								wp->w_compressionType = mToken->compressionType;
								
								pthread_t thread;       
								if (pthread_create(&thread, NULL,  worker, (void*)wp) != 0) 
								{
									perror("Worker thread:");
								}
								else
								{
									FD_CLR(i, &main_set_fd); 
									pthread_detach(thread);
								}
							}
							free(mToken);
						}//if (retv_recv == sizeof(MagicToken))
					}
				}
			} //if (FD_ISSET(i, &read_set_fd))
		} //for(int i = 0; i <= max_fd; i++)
    } //for(;;)

	close(listener);
	return 0;
}//end of file.