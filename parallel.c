#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "os_graph.h"
#include "os_list.h"
#include "os_threadpool.h"

#define MAX_TASK 100
#define MAX_THREAD 4

int sum;
os_graph_t *graph;
os_threadpool_t *thread_pool;
// each try to access graph->visited must be thread-safe
pthread_mutex_t visited_mutex;  // mutex used for accessing graph->visited
pthread_mutex_t sum_mutex;      // mutex used for adding to sum
int nr_tasks;  // used for counting the number of tasks processed

void processNode(void *arg)
{
	unsigned int *nodeIdx = (unsigned int *) arg;

	pthread_mutex_lock(&visited_mutex);
	// if the node has been visited no need to process it
	if (graph->visited[*nodeIdx]) {
		// free the lock
		pthread_mutex_unlock(&visited_mutex);
		return;
	}

	graph->visited[*nodeIdx] = 1;  // mark node as visited
	pthread_mutex_unlock(&visited_mutex);
	os_node_t *node = graph->nodes[*nodeIdx];
	// the add to sum has to be thread-safe
	pthread_mutex_lock(&sum_mutex);
	sum += node->nodeInfo;
	nr_tasks++;  // increment the number of tasks
	pthread_mutex_unlock(&sum_mutex);

	for (int i = 0; i < node->cNeighbours; i++) {
		pthread_mutex_lock(&visited_mutex);
		if (graph->visited[node->neighbours[i]] == 0) {
			pthread_mutex_unlock(&visited_mutex);
			// create new task for the neighbour
			os_task_t *new_task =
					task_create((void *) graph->nodes[node->neighbours[i]], processNode);
			if (new_task != NULL)
				add_task_in_queue(thread_pool, new_task);
		} else {
			pthread_mutex_unlock(&visited_mutex);
		}
	}
}

// traverse the graph and create tasks for the nodes that were not
// visited; this helps process each node
void traverse_graph(void)
{
	for (int i = 0; i < graph->nCount; i++) {
		pthread_mutex_lock(&visited_mutex);
		if (graph->visited[i] == 0) {
			pthread_mutex_unlock(&visited_mutex);
			os_task_t *new_task = task_create((void *) graph->nodes[i], processNode);

			if (new_task != NULL)
				add_task_in_queue(thread_pool, new_task);
		} else {
			pthread_mutex_unlock(&visited_mutex);
		}
	}
}

// function used to determine when the treadpool's processing is done
// the condition is that all the nodes are visited, the number of tasks
// done is equal with the number of nodes and I added the condition that
// the tasks queue is null just to make sure (it must be null if the number
// of processed tasks = number of nodes)
int processingIsDone(os_threadpool_t *tp)
{
	pthread_mutex_lock(&visited_mutex);
	for (int i = 0; i < graph->nCount; i++) {
		if (!graph->visited[i]) {
			pthread_mutex_unlock(&visited_mutex);
			return 0;
		}
	}
	pthread_mutex_unlock(&visited_mutex);
	// condition for stop
	if (nr_tasks == graph->nCount && thread_pool->tasks == NULL)
		return 1;

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: ./main input_file\n");
		exit(1);
	}

	FILE *input_file = fopen(argv[1], "r");

	if (input_file == NULL) {
		printf("[Error] Can't open file\n");
		return -1;
	}

	graph = create_graph_from_file(input_file);
	if (graph == NULL) {
		printf("[Error] Can't read the graph from file\n");
		return -1;
	}
	// initialize the mutexes
	pthread_mutex_init(&visited_mutex, NULL);
	pthread_mutex_init(&sum_mutex, NULL);

	thread_pool = threadpool_create(MAX_TASK, MAX_THREAD);  // create threadpool
	traverse_graph();
	threadpool_stop(thread_pool, processingIsDone);

	printf("%d", sum);
	return 0;
}
