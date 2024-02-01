#include "os_threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

os_task_queue_t *last;// pointer to the last element of the queue
/* === TASK === */

/* Creates a task that thread must execute */
os_task_t *task_create(void *arg, void (*f)(void *))
{
	os_task_t *new_task = (os_task_t *) malloc(sizeof(os_task_t));

	if (new_task == NULL)
		return NULL;
	// init the struct data
	new_task->argument = arg;
	new_task->task = f;
	// return the task
	return new_task;
}

/* Add a new task to threadpool task queue */
void add_task_in_queue(os_threadpool_t *tp, os_task_t *t)
{
	// allocate memory for the task
	os_task_queue_t *new_task =
			(os_task_queue_t *) malloc(sizeof(os_task_queue_t));
	new_task->task = t;
	new_task->next = NULL;
	pthread_mutex_lock(&tp->taskLock);// the enqueue has to be thread-safe
	// I used a pointer to the last element of the queue for the enqueue
	// operation because with an O(n) enqueue implementation some tests would
	// take too much time
	if (tp->tasks == NULL) {
		tp->tasks = new_task;
		last = new_task;
	} else {
		last->next = new_task;
		last = new_task;
	}
	pthread_mutex_unlock(&tp->taskLock);
}

/* Get the head of task queue from threadpool */
os_task_t *get_task(os_threadpool_t *tp)
{
	os_task_t *task = NULL;           // the node which will be returned

	pthread_mutex_lock(&tp->taskLock);// thread-safe operation
	if (tp->tasks == NULL) {
		// free the lock
		pthread_mutex_unlock(&tp->taskLock);
		return NULL;
	}
	task = tp->tasks->task;
	tp->tasks = tp->tasks->next;// move to the next task
	pthread_mutex_unlock(&tp->taskLock);
	return task;
}

/* === THREAD POOL === */

/* Initialize the new threadpool */
os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads)
{
	os_threadpool_t *new_tp = (os_threadpool_t *) malloc(sizeof(os_threadpool_t));

	new_tp->should_stop = 0;
	new_tp->num_threads = nThreads;
	new_tp->threads = (pthread_t *) malloc(nThreads * sizeof(pthread_t));
	new_tp->tasks = NULL;
	pthread_mutex_init(&new_tp->taskLock, NULL);
	// create threads
	for (int i = 0; i < nThreads; i++) {
		pthread_create(&new_tp->threads[i], NULL, thread_loop_function,
					   (void *) new_tp);
	}
	return new_tp;
}

/* Loop function for threads */
void *thread_loop_function(void *args)
{
	os_threadpool_t *tp = (os_threadpool_t *) args;

	while (tp->should_stop == 0) {
		os_task_t *newtask = get_task(tp);// take a task

		if (newtask != NULL)
			// call the task function
			newtask->task(newtask->argument);
	}
	pthread_exit(NULL);
}

/* Stop the thread pool once a condition is met */
void threadpool_stop(os_threadpool_t *tp,
					 int (*processingIsDone)(os_threadpool_t *))
{
	while (!processingIsDone(tp)) {
		// this loop will do
		// busy waiting
	}
	// set should_stop to 1
	tp->should_stop = 1;
	// join threads
	for (int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
}
