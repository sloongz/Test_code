#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include "conn_event.h"

manager_t g_manager;

void event_set(event_t *ev, int fd, void (*call_back)(int, int, void *), void *arg) {

	ev->ev_fd = fd; 
	ev->ev_callback = call_back;
	ev->ev_arg = arg;
	ev->ev_events = 0;
	ev->status = 0;
	ev->is_working = 0;

	pthread_mutex_init(&(ev->work_lock), NULL);
	pthread_cond_init(&(ev->work_run), NULL);
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

	if (ev->status == 1) {
		option = EPOLL_CTL_MOD;
	} else {
		option = EPOLL_CTL_ADD;
		ev->status = 1;
	}   

	if (epoll_ctl(ep_fd, option, ev->ev_fd, &ep_event) < 0) {
		perror("epoll ctl error");
		exit(-1);
	}   
	return ;
}

void event_del(int ep_fd, event_t *ev) {

	int option;

	if (ev->ev_fd == -1) 
	  return ;

	option = EPOLL_CTL_DEL;

	if ((epoll_ctl(ep_fd, option, ev->ev_fd, 0)) < 0) {
		perror("del event error");
		return ;
	}
	pthread_cond_broadcast(&(ev->work_run));
	usleep(1);
	pthread_mutex_destroy(&(ev->work_lock));
	pthread_cond_destroy(&(ev->work_run));

}

void conn_init(connections_t *c, int size)
{
	int i;
	for (i = 0; i < size; i++) {   
		c[i].read = NULL;
		c[i].write = NULL;
	}
}

int conn_set(int fd, int events, void (*call_back)(int, int, void *)) {
	if (fd > g_manager.nconnections) {
		fprintf(stderr, "fd max limits\n");
		return -1; 
	}   

	connections_t *new_conn = &g_manager.conn[fd];
	if (new_conn == NULL) {
		fprintf(stderr, "conn set error\n");
		return -1; 
	}   
	event_t *new_ev = (event_t *)malloc(sizeof(event_t));
	if (new_ev == NULL) {
		fprintf(stderr, "alloc from coon m_cpoll error\n");
		return -1; 
	}   
	void *arg = (void *)new_ev;
	event_set(new_ev, fd, call_back, arg);
	event_add(g_manager.epoll_fd, events, new_ev, 1); 
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
		event_del(g_manager.epoll_fd, c->read);
		event_set(c->read, -1, NULL, 0);
	} else if (c->write != NULL) {
		event_del(g_manager.epoll_fd, c->write);
		event_set(c->write, -1, NULL, 0);
	}
}

