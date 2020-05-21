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
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <errno.h>
#include <libgen.h>

#define MAX_THREAD_NUM 8

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

	int shutdown;
	pthread_t *threadid;

} thread_pool_t;

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

	if (queue->cur_queue_size >= queue->max_thread_num) {
		printf("queue full\n");
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
	} else {
		queue->tail->next = pwork;
	}

	queue->tail = pwork;
	queue->cur_queue_size++;

	return 0;
}

thread_worker_t *queue_dequeue(thread_pool_t *queue)
{
	thread_worker_t *pwork;

	if (queue == NULL)
		return NULL;

	printf("size: %d\n", queue->cur_queue_size);
	if (queue->cur_queue_size <= 0) {
		printf("queue empty\n");
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
	queue_enqueue(pool, work, args);
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
		printf("cur_queue_size:%d\n", pool->cur_queue_size);
		while (pool->cur_queue_size == 0 && !pool->shutdown) {
			printf("thread %ld is waiting\n", pthread_self());
			pthread_cond_wait(&(pool->queue_ready), &(pool->queue_lock));
		}	

		if (pool->shutdown)
		{
			pthread_mutex_unlock(&(pool->queue_lock));
			printf("thread %ld will exit\n", pthread_self());
			pthread_exit (NULL);
		}

		printf("thread %ld is starting to work\n", pthread_self());
		assert(pool->cur_queue_size != 0);
		assert(pool->head != NULL);

		thread_worker_t *worker = queue_dequeue(pool);
		pthread_mutex_unlock(&(pool->queue_lock));

		(*(worker->process_work))(worker->args);
		free(worker);
		worker = NULL;
	}

	pthread_exit(NULL);
}

void *work(void *args)
{
	int i=0;
	while (1) {
		printf("thread %ld working, %d\n", pthread_self(), i++);
		sleep(1);
	}
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
	int i;
	thread_pool_t *pool = NULL;
	pool = threadpool_init(MAX_THREAD_NUM);
	usleep(1000*1);

	for (i=0; i<MAX_THREAD_NUM; i++) {
		threadpool_add_worker(pool, work, NULL);
	}

	while (1);
	threadpool_destroy(pool);

	return 0;
}
