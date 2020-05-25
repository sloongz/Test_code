#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <errno.h>
#include <libgen.h>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 1024
#define UDP_BUFFER_SIZE 1024


int setnonblocking(int fd)
{
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option | O_NONBLOCK;
	fcntl( fd, F_SETFL, new_option );
	return old_option;
}

void addfd(int epollfd, int fd)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	//event.events = EPOLLIN;
	epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
	setnonblocking( fd );
}

int main( int argc, char* argv[] )
{
	if( argc <= 2 )
	{
		printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
		return 1;
	}
	const char* ip = argv[1];
	int port = atoi( argv[2] );
	printf("IP:%s PORT:%d\n", ip, port);

	int ret = 0;

	//====================tcp sock=====================================
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = inet_addr(ip);
	//address.sin_addr.s_addr = htonl(INADDR_ANY);


	int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
	if (tcpfd < 0) {
		perror("tcp sock");	
		return -1;
	}

	int opt=1;
	setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	ret = bind(tcpfd, (struct sockaddr*)&address, sizeof(address));
	if (ret < 0) {
		perror("tcp bind");	
		return -1;
	}

	ret = listen(tcpfd, 10);
	if (ret < 0) {
		perror("tcp listen");
		return -1;
	} 
	//=================tcp sock end=====================================


	//=================udp sock========================================
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (udpfd < 0) {
		perror("udp sock");	
		return -1;
	}

	ret = bind(udpfd, (struct sockaddr*)&address, sizeof(address));
	if (ret < 0) {
		perror("udp bind");	
		return -1;
	}
	//=================udp sock end=====================================

	struct epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(20);
	if (epollfd < 0) {
		perror("epoll_create");	
		return -1;
	}
	addfd(epollfd, tcpfd);
	addfd(epollfd, udpfd);

	while(1)
	{
		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
		if (number < 0)
		{
			printf( "epoll failure\n" );
			break;
		}

		for (int i = 0; i < number; i++)
		{
			int sockfd = events[i].data.fd;
			if (sockfd == tcpfd)
			{
				printf("trgger---> fd:%d evnet:EPOLLIN\n", sockfd);
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof( client_address );
				int connfd = accept(tcpfd, ( struct sockaddr* )&client_address, &client_addrlength);
				printf("accept tcp client addr %s, connfd:%d\n", inet_ntoa(client_address.sin_addr), connfd);
				addfd(epollfd, connfd);
			}
			else if (sockfd == udpfd)
			{
				char buf[UDP_BUFFER_SIZE];
				memset(buf, '\0', UDP_BUFFER_SIZE);
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);

				ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE-1, 0, (struct sockaddr*)&client_address, &client_addrlength);
				if(ret > 0)
				{
					printf("recv udp client addr %s\n", inet_ntoa(client_address.sin_addr));
					sendto(udpfd, buf, UDP_BUFFER_SIZE-1, 0, (struct sockaddr*)&client_address, client_addrlength);
				}
			}
			else if (events[i].events & EPOLLIN) //tcp connfd read event
			{
				char buf[TCP_BUFFER_SIZE];
				while(1)
				{
					memset(buf, '\0', TCP_BUFFER_SIZE);
					ret = recv(sockfd, buf, TCP_BUFFER_SIZE-1, 0);
					if( ret < 0 )
					{
						if( (errno == EAGAIN ) || ( errno == EWOULDBLOCK ))
						{
							break;
						}
						close( sockfd );
						break;
					}
					else if(ret == 0)
					{
						close(sockfd);
					}
					else
					{
						printf("tcp recv %d bytes\n", ret);
						send(sockfd, buf, ret, 0);
					}
				}
			}
			else
			{
				printf( "something else happened \n" );
			}
		}
	}
	close(tcpfd);
	return 0;
}
