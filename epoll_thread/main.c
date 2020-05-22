#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "thread_pool.h"

void *work(void *args)
{
	int num = *(int *)args;
	int i = 30;

	printf("%s, thread %ld num:%d\n", __func__, pthread_self(), num);
	while (i) {
		sleep(1);
		i--;
	}
	return NULL;
}

int main(int argc, char **argv)
{
	int i;
	int arr[10];
	threadpool_t * pool;
	pool = threadpool_create(10, 30, 30);
	sleep(1);

	for (i=0; i<10; i++) {
		arr[i] = i;
		threadpool_add(pool, work, (void *)&arr[i]);
	}

	sleep(2);
	for (i=0; i<10; i++) {
		arr[i] = i+10;
		threadpool_add(pool, work, (void *)&arr[i]);
	}
	while (1);
	threadpool_destroy(pool);

	return 0;
}
