// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;

/* TODO: Define graph synchronization mechanisms. */
static pthread_mutex_t sum_mutex;
static pthread_mutex_t *graph_mutex;

/* TODO: Define graph task argument. */
typedef struct {
	os_graph_t *graph;
	unsigned int node_idx;
} graph_task_arg_t;

static void process_graph_node(void *arg)
{
	graph_task_arg_t *graph_arg = (graph_task_arg_t *)arg;
	os_node_t *node = graph_arg->graph->nodes[graph_arg->node_idx];

	pthread_mutex_lock(&sum_mutex);
	sum += node->info;
	pthread_mutex_unlock(&sum_mutex);

	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		pthread_mutex_lock(&graph_mutex[graph_arg->node_idx]);
		if (graph->visited[node->neighbours[i]] == NOT_VISITED) {
			graph->visited[node->neighbours[i]] = PROCESSING;

			graph_task_arg_t *new_arg = malloc(sizeof(graph_task_arg_t));

			DIE(new_arg == NULL, "malloc");

			new_arg->graph = graph;
			new_arg->node_idx = node->neighbours[i];

			os_task_t *new_task = create_task(process_graph_node, new_arg, free);

			enqueue_task(tp, new_task);
		}
		pthread_mutex_unlock(&graph_mutex[graph_arg->node_idx]);
	}

	pthread_mutex_lock(&graph_mutex[graph_arg->node_idx]);
	graph->visited[graph_arg->node_idx] = DONE;
	pthread_mutex_unlock(&graph_mutex[graph_arg->node_idx]);
}

static void process_node(unsigned int idx)
{
	/* TODO: Implement thread-pool based processing of graph. */
	graph_task_arg_t *arg = malloc(sizeof(graph_task_arg_t));

	DIE(arg == NULL, "malloc");

	arg->graph = graph;
	arg->node_idx = idx;

	graph->visited[idx] = PROCESSING;

	os_task_t *task = create_task(process_graph_node, arg, free);

	enqueue_task(tp, task);
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* TODO: Initialize graph synchronization mechanisms. */
	pthread_mutex_init(&sum_mutex, NULL);
	pthread_mutex_init(&graph_mutex, NULL);

	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	pthread_mutex_destroy(&sum_mutex);
	pthread_mutex_destroy(&graph_mutex);

	printf("%d", sum);

	return 0;
}
