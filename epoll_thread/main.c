#include <sys/types.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <errno.h>
#include <libgen.h>


#include "thread_pool.h"
#include "conn_event.h"

#define MAX_EVENT_NUMBER 4096
#define SERV_PORT   2015

void *read_work(void *arg)
{
	event_t *ev = (event_t *)arg;
	char buf[2048];
	int sock_fd = ev->ev_fd;
	int ret;
	int i;

	while (1) {
		printf("%s\n", __func__);
		pthread_cond_wait(&(ev->work_run), &(ev->work_lock));
		printf("%s, go...\n", __func__);
		memset(buf, '\0', 2048);

		ret = recv(sock_fd, buf, 2048, 0); 
		if( ret <= 0 ) {
			if (errno == ECONNRESET) {     
				printf("errno ECONRESET!\n");   
			}
			if (errno == EBADF) {          
				printf("errno EBADF\n");       
			}
			if( (errno != EAGAIN ) || ( errno != EWOULDBLOCK ) /*|| errno != EINTR*/) {
				printf("%s %d, recv error\n", __func__, __LINE__);
				perror("tcp recv");            
				break;
			}
		}// else if(ret == 0) {        
		// perror("tcp recv");         
		// printf("%s %d, recv error\n", __func__, __LINE__);
		// close(sock_fd);
		//} 
		else {
			printf("tcp recv %d bytes:%s\n", ret, buf);
			sprintf(buf, "working, connfd=%d, cnt:%d\n", sock_fd, i++);
			ret = send(sock_fd, buf, strlen(buf), 0);
			if (ret < 0) {
				perror("tcp send:");
			}
		}
	}

    pthread_mutex_lock(&(g_manager.conn_lock));
	conn_free(sock_fd);
	close(sock_fd);
	pthread_mutex_unlock(&(g_manager.conn_lock));

	return NULL;
}


void process_conn(int sock_fd, int events, void *arg)
{
	event_t *ev = (event_t *)arg;
	printf("%s, add connfd:%d, ev->ev_fd:%d work to thread pool\n", __func__, sock_fd, ev->ev_fd);
	if (ev->is_working) {
		printf("is_working\n");	
		pthread_cond_signal(&(ev->work_run));
	} else {
		threadpool_add(g_manager.pool, read_work, (void *)ev);
		ev->is_working = 1;
	}
}


void *accept_connect(void *arg)
{
	event_t *ev = (event_t *)arg;
	struct sockaddr_in client_addr;
	socklen_t client_len;
	client_len = sizeof(client_addr);

	printf("%s, fd:%d\n", __func__, ev->ev_fd);
	int new_fd = accept(ev->ev_fd, (struct sockaddr* )&client_addr, &client_len);
	if (new_fd < 0) {
		perror("accept error");
	}   
	conn_set(new_fd, EPOLLIN, (void *)process_conn);
	printf("accept tcp client addr %s, connfd:%d\n", inet_ntoa(client_addr.sin_addr), new_fd);
	printf("work accept_connect func exit, thread:\033[1m\033[43;33m%ld\033[0m\n", pthread_self());

	return NULL;
}

void add_conn(int sock_fd, int events, void *arg)
{
	event_t *ev = (event_t *)arg;
	printf("%s, add accept_connect func to thread pool. tcpfd:%d, is_working:%d\n", __func__, ev->ev_fd, ev->is_working);
	threadpool_add(g_manager.pool, accept_connect, (void *)ev);
}


int main(int argc, char **argv)
{
	g_manager.pool = threadpool_create(10, 30, 30);
	usleep(1000*1);
	g_manager.conn = (connections_t *)malloc(sizeof(connections_t )*MAX_EVENT_NUMBER);
	conn_init(g_manager.conn, MAX_EVENT_NUMBER);
	pthread_mutex_init(&(g_manager.conn_lock), NULL);

	g_manager.epoll_events = (struct epoll_event *)malloc(sizeof(struct epoll_event)*MAX_EVENT_NUMBER);
	g_manager.epoll_fd = epoll_create(MAX_EVENT_NUMBER);
	g_manager.max_ep_events = MAX_EVENT_NUMBER;
	g_manager.nconnections = MAX_EVENT_NUMBER;
	if (g_manager.epoll_fd < 0) {
		perror("epoll create error");  
		return -1;            
	}  
	printf("epoll_fd:%d\n", g_manager.epoll_fd);

	int ret;
	int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
	if (tcpfd < 0) {
		perror("tcp sock");
		return -1;
	}
	printf("sockfd:%d\n", tcpfd);

	int opt=1;
	setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	//struct linger optl;
	//optl.l_onoff = 1;
	//optl.l_linger = 60; 
	//setsockopt(tcpfd, SOL_SOCKET, SO_LINGER, &optl, sizeof(struct linger));

	struct sockaddr_in serv_addr;
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.227");
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(tcpfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if (ret < 0) {
		perror("tcp bind");
		return -1;
	}

	ret = listen(tcpfd, 30);
	if (ret < 0) {
		perror("tcp listen");
		return -1;
	}

	if (conn_set(tcpfd, EPOLLIN, (void *)add_conn) < 0) {
		fprintf(stderr, "listen fd:conn set error\n");
		return -1;
	}

	while (1) {
		int event_count = epoll_wait(g_manager.epoll_fd, g_manager.epoll_events, g_manager.max_ep_events, -1);
		if(event_count < 0) {
			perror("epoll wait error");
			return -1;  
		}   
		int i=0;
		for (i=0; i<event_count; i++) {
			int fd = g_manager.epoll_events[i].data.fd;
			int events = g_manager.epoll_events[i].events;
			connections_t *ev_conn = &g_manager.conn[fd];
			if (fd == tcpfd) {
				printf("trgger---> fd:%d evnet:EPOLLIN\n", fd);
			}

			if (events & EPOLLIN) {
				if (ev_conn->read) {
					event_t *read_ev = ev_conn->read;
					read_ev->ev_callback(read_ev->ev_fd, read_ev->ev_events, read_ev->ev_arg);	
				}
			}
			if (events & EPOLLOUT) {
				if (ev_conn->write) {
					event_t *write_ev= ev_conn->write;
					write_ev->ev_callback(write_ev->ev_fd, write_ev->ev_events, write_ev->ev_arg);
				}
			}
		}//for()

	}

	return 0;
}
