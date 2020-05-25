#ifndef _CONN_EVENT_H_
#define _CONN_EVENT_H_

#include <stdbool.h>
#include <pthread.h>
#include "thread_pool.h"

typedef enum ev_status {
	EV_ADD,
	EV_MOD
} ev_status_t;

typedef enum ev_block {
	EV_BLOCK,
	EV_NOBLOCK
} ev_block_t;

typedef struct event {
	int ev_fd;
	void (*ev_callback)(int, int, void *ev_arg);
	void *ev_arg;
	int ev_events;
	ev_status_t status;
	bool is_working;
	pthread_cond_t work_run;
	pthread_mutex_t work_lock;
} event_t;

typedef struct connections {
	event_t *read;
	event_t *write;
} connections_t;

typedef struct manager {
	int epoll_fd;
	struct epoll_event *epoll_events;
	int max_ep_events;

	connections_t *conn;
	int nconnections;

	threadpool_t * pool;
	pthread_mutex_t conn_lock;
} manager_t;


extern manager_t g_manager;

void event_set(event_t *ev, int fd, void (*call_back)(int, int, void *), void *arg);
void event_add(int ep_fd, int events, event_t *ev, int et_flag);
void event_del(int ep_fd, event_t *ev);
void conn_init(connections_t *c, int size);
int conn_set(int fd, int events, void (*call_back)(int, int, void *));
void conn_free(int fd);

#endif
