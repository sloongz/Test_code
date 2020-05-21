#include<stdlib.h>
#include<stdio.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <pthread.h>
#include<errno.h>

#define PORT 2015
#define BUFF_LEN 512

int sockfd;

void *recv_func(void* arg)
{
	char buf[BUFF_LEN];
	int len;
	
	while (1) {
		len = recv(sockfd, buf, BUFF_LEN, 0);
		if (len > 0) {
			printf("recv server: %s\n", buf);
		} else if (len < 0) {
			perror("recv data error\n");
			close(sockfd);
			break;
		} else {
			printf("len = 0\n");
		}
	}
	return NULL;
}

int main(int argc,char** argv)
{
	struct sockaddr_in ser_addr;
	char buf[BUFF_LEN] = {};
	pthread_t recv_id;
	int port;
	int ret;
	int i;

	if ((argc<2)|| (argc>3)) {
		return 0;
	}

	if (argc==3) {
		port = atoi(argv[2]);
	}

	sockfd=socket(AF_INET,SOCK_STREAM,0);
	if (sockfd < 0) {
		perror("can not create socket\n");
		return 0;
	}

	memset(&ser_addr,0,sizeof(struct sockaddr_in));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_addr.s_addr = inet_addr(argv[1]);
	ser_addr.sin_port = htons(port);

	ret = connect(sockfd,(struct sockaddr*)&ser_addr,sizeof(struct sockaddr));
	if (ret < 0) {
		perror("connect error");
		close(sockfd);
		return -1;
	}

	pthread_create(&recv_id, NULL, (void *)recv_func, NULL);

	while(1)
	{
		memset(buf, 0, BUFF_LEN);
		sprintf(buf, "seq:%d, client tcp msg\n", i++);
		send(sockfd, buf, strlen(buf), 0);
		sleep(1);  //一秒发送一次消息
	}

	pthread_join(recv_id, NULL);
	close(sockfd);

	return 0;
}
