#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include<errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>


#define SERVER_PORT 2015
#define BUFF_LEN 512

int client_fd;
struct sockaddr_in ser_addr;

void *recv_func(void* arg)
{
	char buf[BUFF_LEN];
	int addrlen = sizeof(ser_addr);
	while (1) {
		memset(buf, 0, BUFF_LEN);
		recvfrom(client_fd, buf, BUFF_LEN, 0, (struct sockaddr*)&ser_addr, &addrlen);  //接收来自server的信息
		printf("ipaddr:%s, port:%d\n", inet_ntoa(ser_addr.sin_addr), ntohs(ser_addr.sin_port));
		printf("recv:%s\n", buf);
	}
}

int main(int argc, char **argv)
{
	int ret;
	int i = 0;
	char buf[BUFF_LEN];
	char *SERVER_IP;

	pthread_t recv_id;
	SERVER_IP = argv[1];
	printf("ip:%s\n", SERVER_IP);

	client_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(client_fd < 0) {
		printf("create socket fail!\n");
		return -1; 
	}

	memset(&ser_addr, 0, sizeof(ser_addr));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	//ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //注意网络序转换
	ser_addr.sin_port = htons(SERVER_PORT);  //注意网络序转换

	bind(client_fd, (struct sockaddr *)&ser_addr, sizeof(ser_addr));
	//connect(client_fd, (struct sockaddr* )&ser_addr, sizeof(ser_addr));

	pthread_create(&recv_id, NULL, (void *)recv_func, NULL);

	//sleep(10);
	while(1)
	{
		memset(buf, 0, BUFF_LEN);
		sprintf(buf, "seq:%d, client udp msg\n", i++);
		//printf("client:%s\n",buf);  //打印自己发送的信息
		sendto(client_fd, buf, BUFF_LEN, 0, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
		sleep(1);  //一秒发送一次消息
	}

	pthread_join(recv_id, NULL);
	close(client_fd);

	return 0;
}

