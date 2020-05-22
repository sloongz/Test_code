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

#define MAX_THREAD_NUM 10
#define MAX_EVENT_NUMBER 1024
#define SERV_PORT   2015

typedef struct thread_worker {
	void *(*process_work) (void *args);
	void *args;
	int val;
	struct thread_worker *next;	
} thread_worker_t;

typedef struct thread_pool {
	pthread_mutex_t queue_lock;
	pthread_cond_t queue_ready;

	thread_worker_t *head;
	thread_worker_t *tail;
	int cur_queue_size;	
	int max_thread_num;
	int working_cnt;

	int shutdown;
	pthread_t *threadid;

} thread_pool_t;


typedef struct event {
	int ev_fd;
	void (*ev_callback)(int, int, void *ev_arg);
	void *ev_arg;
	int ev_events;
	int ev_status;
} event_t;


typedef struct connections {
	bool used_bit;
	event_t *read;
	event_t *write;
	event_t *rdhup;
} connections_t;

typedef struct manager {
	int ep_fd;
	struct epoll_event *ep_events;
	int max_ep_events;

	connections_t *conn;
	int nconnections;

	thread_pool_t *pool;

	pthread_mutex_t conn_lock;
} manager_t;


manager_t g_manager;


void event_set(event_t *ev, int fd, void (*call_back)(int, int, void *), void *arg);
void event_add(int ep_fd, int events, event_t *ev, int et_flag);
void event_del(int ep_fd, event_t *ev);
void conn_init(connections_t *c, int size);
int conn_set(int fd, int events, void (*call_back)(int, int, void *));
void conn_free(int fd);
void add_conn(int sock_fd, int events, void *arg);
void *accept_connect(void *arg);
void process_conn(int sock_fd, int events, void *arg);

thread_pool_t *queue_init(int max_num);
int queue_free(thread_pool_t *queue);
int queue_enqueue(thread_pool_t *queue, void *work, void *args);
thread_worker_t *queue_dequeue(thread_pool_t *queue);
int threadpool_add_worker(thread_pool_t *pool, void *(*work)(void *args), void *args);
int threadpool_destroy(thread_pool_t *pool);
void* thread_routine(void *args);
thread_pool_t *threadpool_init( int max_thread_num);

void *work(void *args);

void event_set(event_t *ev, int fd, void (*call_back)(int, int, void *), void *arg)
{
	ev->ev_fd = fd;
	ev->ev_callback = call_back;
	ev->ev_arg = arg;
	ev->ev_events = 0;
	ev->ev_status = 0;

	return ;
}

void event_add(int ep_fd, int events, event_t *ev, int et_flag)
{
	struct epoll_event ep_event = {0, {0}};
	int option;

	if (et_flag) {
		fcntl(ev->ev_fd, F_SETFL, O_NONBLOCK);
		events |= EPOLLET;
	}

	ep_event.data.fd = ev->ev_fd;
	ep_event.events = ev->ev_events = events;

	printf("%s,ep_fd:%d ", __func__, ep_fd);
	if (ev->ev_status == 1) {
		option = EPOLL_CTL_MOD;
		printf("EPOLL_CTL_MOD ");
	} else {
		option = EPOLL_CTL_ADD;
		printf("EPOLL_CTL_ADD ");
		ev->ev_status = 1;
	}

	if (ep_event.events & EPOLLIN) {
		printf("EPOLLIN ");
    }
	if (ep_event.events & EPOLLOUT) {
		printf("EPOLLOUT ");
    }
	if (ep_event.events & EPOLLRDHUP) {
		printf("EPOLLRDHUP ");
    }
	printf("fd:%d\n", ev->ev_fd);

	if (epoll_ctl(ep_fd, option, ev->ev_fd, &ep_event) < 0) {
		perror("epoll ctl error");
		return;
	}
	return ;
}

void event_del(int ep_fd, event_t *ev)
{

	int option;

	if (ev->ev_fd == -1)
	  return ;

	option = EPOLL_CTL_DEL;

	if ((epoll_ctl(ep_fd, option, ev->ev_fd, 0)) < 0) {
		perror("del event error");
		return ;
	}
}

void conn_init(connections_t *c, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		c[i].used_bit = 0;
		c[i].read = NULL;
		c[i].write = NULL;
		c[i].rdhup = NULL;
	}
}

int conn_set(int fd, int events, void (*call_back)(int, int, void *))
{
	connections_t *new_conn = &g_manager.conn[fd];
	if (new_conn == NULL) {
		fprintf(stderr, "conn set error\n");
		return -1; 
	}   
	
	printf("%s %d, set fd:%d\n", __func__, __LINE__, fd);

	event_t *new_ev = (event_t *)malloc(sizeof(event_t));
	if (new_ev == NULL) {
		fprintf(stderr, "alloc from coon m_cpoll error\n");
		return -1; 
	}

	event_set(new_ev, fd, call_back, (void *)new_ev);
	event_add(g_manager.ep_fd, events, new_ev, 1); 
	if (events & EPOLLIN) {
		new_conn->read = new_ev;
	}   
	if (events & EPOLLOUT) {
		new_conn->write = new_ev;
	}   

	return 0;
} 

void conn_free(int fd)
{
	if (fd == -1)
	  return;

	connections_t *c = &g_manager.conn[fd];
	if (c == NULL)
	  return ;

	if (c->read != NULL) {
		event_del(g_manager.ep_fd, c->read);
		event_set(c->read, -1, NULL, 0);
	} else if (c->write != NULL) {
		event_del(g_manager.ep_fd, c->write);
		event_set(c->write, -1, NULL, 0);
	}
	c->used_bit = 0;
}

void add_conn(int sock_fd, int events, void *arg)
{
	printf("%s, add accept_connect func to thread pool. tcpfd:%d\n", __func__, sock_fd);
	int *tcpfd = (int *)malloc(sizeof(int));
	*tcpfd = sock_fd;
	//printf("%d\n", *tcpfd);
	threadpool_add_worker(g_manager.pool, accept_connect, (void *)tcpfd);
}


void *accept_connect(void *arg)
{
	int tcpfd = *(int *)arg;
	struct sockaddr_in client_addr;
	socklen_t client_len;
	client_len = sizeof(client_addr);

	pthread_mutex_lock(&(g_manager.pool->queue_lock));
	printf("working_cnt:%d, nconnections:%d\n", g_manager.pool->working_cnt, g_manager.nconnections);	
	if (g_manager.pool->working_cnt >= MAX_THREAD_NUM || g_manager.nconnections >= MAX_THREAD_NUM) {
		printf("thread is full, can not accept connect, working_cnt:%d, nconnections:%d\n", g_manager.pool->working_cnt, g_manager.nconnections);	
		return NULL;
	}  
	g_manager.nconnections++;
	pthread_mutex_unlock(&(g_manager.pool->queue_lock));
	int new_fd = accept(tcpfd, (struct sockaddr* )&client_addr, &client_len);
	if (new_fd < 0) {
		perror("accept error");
		return NULL;
	}
	printf("accept tcp client addr %s, connfd:%d\n", inet_ntoa(client_addr.sin_addr), new_fd);

	conn_set(new_fd, EPOLLIN | EPOLLRDHUP, (void *)process_conn);

	printf("work accept_connect func exit, thread:\033[1m\033[43;33m%ld\033[0m\n", pthread_self());
	return NULL;
}

void process_conn(int sock_fd, int events, void *arg)
{
	printf("%s, add connfd:%d work to thread pool\n", __func__, sock_fd);
	int *tcpfd = (int *)malloc(sizeof(int));
	*tcpfd = sock_fd;
	threadpool_add_worker(g_manager.pool, work, (void *)tcpfd);
	g_manager.conn[sock_fd].used_bit = 1;
}


thread_pool_t *queue_init(int max_num)
{
	thread_pool_t *queue;

	queue = (thread_pool_t *)malloc(sizeof(thread_pool_t));
	if (queue == NULL)
	  return NULL;

	queue->head = NULL;
	queue->tail = NULL;
	queue->cur_queue_size = 0;
	queue->max_thread_num = max_num;

	queue->working_cnt = 0;

	return queue;
}

int queue_free(thread_pool_t *queue)
{
	thread_worker_t *pwork;

	if (queue == NULL)
	  return -1;
	while (queue->head != NULL) {
		pwork = queue->head;
		queue->head = queue->head->next;
		free(pwork);
	}
	free(queue);

	return 0;
}

int queue_enqueue(thread_pool_t *queue, void *work, void *args)
{
	thread_worker_t *pwork;

	printf("%s\n", __func__);
	if (queue->cur_queue_size >= queue->max_thread_num) {
		printf("queue full, cur_queue_size:%d\n", queue->cur_queue_size);
		return -1;
	}

	pwork = (thread_worker_t *)malloc(sizeof(thread_worker_t));
	if (pwork == NULL)
	  return -1;
	pwork->process_work = work;
	pwork->args = args;
	pwork->next = NULL;

	if (queue->head == NULL) {
		queue->head = pwork;
		pwork->val = 1;
	} else {
		queue->tail->next = pwork;
		pwork->val++;
	}

	queue->tail = pwork;
	queue->cur_queue_size++;

	return 0;
}

thread_worker_t *queue_dequeue(thread_pool_t *queue)
{
	thread_worker_t *pwork;

	printf("%s\n", __func__);
	if (queue == NULL)
		return NULL;

	if (queue->cur_queue_size <= 0) {
		printf("queue empty, cur_queue_size:%d\n", queue->cur_queue_size);
		return NULL;
	} 

	pwork = queue->head;
	if (pwork == NULL) {
		printf("queue empty\n");	
		return NULL;
	}
	queue->head = pwork->next;

	queue->cur_queue_size--;

	return pwork;
}


int threadpool_add_worker(thread_pool_t *pool, void *(*work)(void *args), void *args)
{
	pthread_mutex_lock(&(pool->queue_lock)); 
	printf("%s func address:%p\n", __func__, work);
	queue_enqueue(pool, work, args);
	pool->working_cnt++;
	pthread_mutex_unlock(&(pool->queue_lock));
	pthread_cond_signal(&(pool->queue_ready));

	return 0;
}


int threadpool_destroy(thread_pool_t *pool)
{
	if (pool->shutdown)
	  return -1;

	pool->shutdown = 1;

	pthread_cond_broadcast(&(pool->queue_ready));

	int i;
	for (i=0; i<pool->max_thread_num; i++) {
		pthread_join(pool->threadid[i], NULL);
	}

	free(pool->threadid);
	pool->threadid = NULL;

	queue_free(pool);

	pthread_mutex_destroy(&(pool->queue_lock));
	pthread_cond_destroy(&(pool->queue_ready));
	pool=NULL;

	return 0;
}

void* thread_routine(void *args)
{	
	thread_pool_t *pool = args;

	while (1) {    
		pthread_mutex_lock(&(pool->queue_lock));
		while ((pool->cur_queue_size == 0 || pool->working_cnt == 0) && !pool->shutdown) {
			printf("thread %ld is waiting..., working_cnt:%d\n", pthread_self(), pool->working_cnt);
			pthread_cond_wait(&(pool->queue_ready), &(pool->queue_lock));
			printf("thread %ld is go...\n", pthread_self());
		}	

		if (pool->shutdown) {
			printf("=============!!!! shutdown !!!!===============\n");
			pthread_mutex_unlock(&(pool->queue_lock));
			printf("thread %ld will exit\n", pthread_self());
			pthread_exit (NULL);
		}

		printf("thread \033[1m\033[42;33m%ld \033[0m is starting to work\n", pthread_self());

		thread_worker_t *worker = queue_dequeue(pool);
		printf("working_cnt:%d\n", pool->working_cnt);
		pthread_mutex_unlock(&(pool->queue_lock));

		if (worker) {
			(*(worker->process_work))(worker->args);

			pthread_mutex_lock(&(pool->queue_lock));
			printf("!!! work exit, working_cnt:%d, thread:\033[1m\033[42;33m%ld\033[0m, func address:%p\n", pool->working_cnt, pthread_self(), worker->process_work);
			pool->working_cnt--;
			pthread_mutex_unlock(&(pool->queue_lock));

			free(worker);
			worker = NULL;
		} else {
			printf("no worker..................\n");	
		}
	}

	pthread_exit(NULL);
}

void *work(void *args)
{
	int sock_fd = *(int *)args;
	char buf[1024];
	int ret;
	int i = 0;

	printf("thread \033[1m\033[42;33m%ld \033[0m working, connfd=%d\n\n\n", pthread_self(), sock_fd);

	while (1) {
		memset(buf, '\0', 1024);
		ret = recv(sock_fd, buf, 1024, 0);
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
		 //	perror("tcp recv");
		 //	printf("%s %d, recv error\n", __func__, __LINE__);
		 //	close(sock_fd);
		 //} 
		else {
			//printf("tcp recv %d bytes:%s\n", ret, buf);
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
	g_manager.nconnections--;
	pthread_mutex_unlock(&(g_manager.conn_lock));

	printf("thread %ld func work func exit!!!\n", pthread_self());
	return NULL;
}


thread_pool_t *threadpool_init( int max_thread_num)
{
	int i = 0;

	thread_pool_t *pool = queue_init(max_thread_num);
	pthread_mutex_init(&(pool->queue_lock), NULL);
	pthread_cond_init(&(pool->queue_ready), NULL);
	pool->threadid = (pthread_t *)malloc(max_thread_num * sizeof(pthread_t));

	for (i=0; i<max_thread_num; i++) { 
		pthread_create(&(pool->threadid[i]), NULL, thread_routine, pool);
	}

	return pool;
}


int main(int argc, char **argv)
{
	int ret = 0;
	g_manager.pool = threadpool_init(MAX_THREAD_NUM);
	usleep(1000*1);

	g_manager.conn = (connections_t *)malloc(sizeof(connections_t )*MAX_EVENT_NUMBER);
	g_manager.nconnections = 0;
	conn_init(g_manager.conn, MAX_EVENT_NUMBER);

	pthread_mutex_init(&(g_manager.conn_lock), NULL);

	g_manager.ep_events = (struct epoll_event *)malloc(sizeof(struct epoll_event)*MAX_EVENT_NUMBER);
	g_manager.ep_fd = epoll_create(MAX_EVENT_NUMBER);
	g_manager.max_ep_events = 4096;
	if (g_manager.ep_fd < 0) {
		perror("epoll create error");
		return -1; 
	} 
	printf("ep_fd:%d\n", g_manager.ep_fd);

	//if( argc <= 2 )
	//{
	//	printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
	//	return 1;
	//}
	//const char* ip = argv[1];
	//int port = atoi( argv[2] );
	//printf("IP:%s PORT:%d\n", ip, port);


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

	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(SERV_PORT);
	address.sin_addr.s_addr = inet_addr("192.168.1.227");
	//address.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(tcpfd, (struct sockaddr*)&address, sizeof(address));
	if (ret < 0) {
		perror("tcp bind");
		return -1;
	}

	ret = listen(tcpfd, 20);
	if (ret < 0) {
		perror("tcp listen");
		return -1;
	}

	if (conn_set(tcpfd, EPOLLIN, NULL) < 0) {
		fprintf(stderr, "listen fd:conn set error\n");
		return -1;
	} 
	
	while (1) {
		int event_count = epoll_wait(g_manager.ep_fd, g_manager.ep_events, g_manager.max_ep_events, -1);
		if(event_count < 0) {
			perror("epoll wait error");
			return -1;	
		}
		int i=0;
		for (i=0; i<event_count; i++) {
			int fd = g_manager.ep_events[i].data.fd;
			int events = g_manager.ep_events[i].events;
			connections_t *ev_conn = &g_manager.conn[fd];

			if (ev_conn == NULL) {
				printf("ev_conn = 0\n");
				continue;
			}
			printf("epoll event: 0x%x, EPOLLRDHUP:0x%x, EPOLLIN:0x%x, EPOLLOUT:0x%x\n", events, EPOLLRDHUP, EPOLLIN, EPOLLOUT);
			if (events & EPOLLRDHUP) {
				printf("epoll event EPOLLRDHUP\n");
			}
			if (events & EPOLLERR) {
				printf("epoll event EPOLLERR\n");	
			}
			if (events & EPOLLIN) {
				if (fd == tcpfd) {
					printf("%s %d accept\n", __func__, __LINE__);			
					accept_connect((void *)&fd);
				}
				if (ev_conn->read) {
					event_t *read_ev = ev_conn->read;	
					printf("epoll read event fd: %d, events:%x\n", read_ev->ev_fd, read_ev->ev_events);	
					if (g_manager.conn[fd].used_bit) {
						continue;
					}
					if (read_ev->ev_callback) { 
						read_ev->ev_callback(read_ev->ev_fd, read_ev->ev_events, read_ev->ev_arg);
					} else {
						printf("call back NULL\n");
					}
				}   
			}   
			if (events & EPOLLOUT) {
				if (ev_conn->write) {
					event_t *write_ev= ev_conn->write;
					write_ev->ev_callback(write_ev->ev_fd, write_ev->ev_events, write_ev->ev_arg);
				}   
			}

		}
	}

	threadpool_destroy(g_manager.pool);
	free(g_manager.conn);

	return 0;
}
