/*Assignment2FinalEdition.c
Written by Ben Jollymore and Johnathan Power

Passes series of connected nodes through Floyd Warshall
algorithm to compute shortest distance between nodes

Algorithm is implemented using POSIX threads.

Tested on Ubuntu 18.04.01LTS
Compiled using GCC
Last version updated as of 23:34-27-10-2018
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <string.h>

/*from pthread_create manual page https://www.systutorials.com/docs/linux/man/3-pthread_create*/
#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define INF 99999999;

//data for thread to work with
typedef struct {
	int nodeQuantity; 	//number of nodes for loop
	int i;				//i val for loop
	int k;				//k val for loop
	int **dist;			//dist array
} thread_data;

//global 2d distance matrix
int **dist;

//global array of threads
pthread_t * threadArray;

//readwrite mutex lock
pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;

void segfaultCatch(int sig, siginfo_t *seg_sig_info, void *arg)
{
    printf("Negative edge causing an out of bounds error. Program now exitting due corrupted environment %p\n", seg_sig_info->si_addr);
    exit(0);
}

//read data using rdlock syncronization
int readHelper(int i, int j, int k, int **inputData) {
	int result;
	pthread_rwlock_rdlock(&lock);
	result = ((inputData[i][k] + inputData[k][j]) < inputData[i][j]);
	pthread_rwlock_unlock(&lock);
	return result;
}

//calculation segment for threads
void *runner(void *inputData) {
	//unpack data for use in thread
	thread_data *data = (thread_data*)inputData;
	data = inputData;
	int i = data->i;
	int k = data->k;
	int **dist = data->dist;
	int nodes = data->nodeQuantity;

	for (int j = 1; j <= nodes; j++)
	{
		if (readHelper(i, j, k, dist))
		{
			//write to data using wrlock syncronziation
			pthread_rwlock_wrlock(&lock);
			printf("thread i %d in shell k=%d has writelocked and is processing\n", i, k);
			dist[i][j] = dist[i][k] + dist[k][j];
			printf("updating shortest path link for node [%d][%d] to %d from INF\n", i, j, dist[i][j]);
			pthread_rwlock_unlock(&lock);
			printf("thread i %d in shell k=%d has unlocked write\n", i, k);
			printf("--------------------------------------------------------------------------------\n");
		}
	}
	return NULL;
}


//create thread, pass it an id (used for indexing in array, not actual tid), and an array to put it in,
//as well as void pointer to data it can work with
void createThreads(thread_data *data, int id) {
	int thread;
	thread = pthread_create(&threadArray[id], NULL, runner, (void *)data);
	if (thread != 0) {
		handle_error_en(thread, "pthread_create");
	}
}

//join the threads
void joinThreads(int numThreads) {
	int thread;
	while (numThreads >= 0) {
		thread = pthread_join(threadArray[numThreads], NULL);
		if (thread != 0) {
			handle_error_en(thread, "pthread_create");
		}
		numThreads--;
	}
}

int main()
{
	int nodes = 0;
	int undirectedEdges = 0;
	//segment error catching init
	struct sigaction sigAct;
    memset(&sigAct, 0, sizeof(struct sigaction));
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_sigaction = segfaultCatch;
    sigAct.sa_flags   = SA_SIGINFO;
    sigaction(SIGSEGV, &sigAct, NULL);
	//end of init
	//Enter the number of nodes with validated input
	while (nodes <= 0) {
		printf("Enter the number of nodes: ");
		scanf("%d", &nodes);
		if (nodes <= 0) {
			printf("There must at least be 1 node. Please enter a valid value.\n");
		}
	}

	printf("--------------------------------------------------------------------------------\n");
	
	//Enter number of edges with validated input
	while (undirectedEdges <= 0) {
		printf("Enter the number of undirected edges: ");
		scanf("%d", &undirectedEdges);
		if (undirectedEdges <= 0) {
			printf("There must at least be 1 edge. Please enter a valid value.\n");
		}
	}

	printf("--------------------------------------------------------------------------------\n");

	//now that number of nodes are known, number of active threads are known.
	//array declared to shove all of those into.
	threadArray = (pthread_t *)malloc(sizeof(pthread_t)*nodes);

	//initialize the matrix of weighted edges between nodes
	int ** edges = (int **)malloc(sizeof(int*)*undirectedEdges);
	for (int i = 0; i < undirectedEdges; i++)
		edges[i] = (int *)malloc(sizeof(int)*3);

	//initialize the matrix of shortest distances
	int **dist = (int **)malloc(sizeof(int *)*nodes+1);
	for (int i = 1; i <= nodes; i++) {
		dist[i] = (int *)malloc(sizeof(int)*nodes+1);
	}

	//Give values to edges. 0 if i == j, else change to INF
	for (int i = 1; i <= nodes; i++)
	{
		for (int j = 1; j <= nodes; j++)
		{
			if (i == j)
				dist[i][j] = 0;
			else
				dist[i][j] = INF;
		}
	}

	//Enter the edges (1 2 1, 2 3 1, 3 4 1, 4 1 1, etc)
	for (int i = 0; i < undirectedEdges; i++)
	{
		//todo: add input validation here
		while(1){
			printf("Enter two nodes and a non-negative edge weight between them\nUsing format <node> <node> <weight>: ");
			scanf("%d %d %d", &edges[i][0], &edges[i][1], &edges[i][2]);
			if(edges[i][2] <= 0){
				printf("Please enter a non-negative weight.\n");
			} else {
				break;
			}
		}
		dist[edges[i][0]][edges[i][1]] = edges[i][2];
		dist[edges[i][1]][edges[i][0]] = edges[i][2];
	}
	printf("--------------------------------------------------------------------------------\n");
	//1 microsecond interupt/sleep. Prevent threads for accessing same information

	struct timespec time = {0, 1};
	//fw alg
	for (int k = 1; k <= nodes; k++)
	{
		for (int i = 1; i <= nodes; i++)
		{
			//assemble data to be passed to thread
			thread_data data;
			data.nodeQuantity = nodes;
			data.i = i;
			data.k = k;
			data.dist = dist;
			createThreads(&data, i - 1);
			nanosleep(&time, NULL);
		}

		//Call our thread joining function passing # of threads which happens to be #nodes-1
		joinThreads(nodes-1);
	}

	printf("Final Matrix of Shortest Distance: \n");
	//print matrix
	for (int i = 1; i <= nodes; i++)
	{
		for (int j = 1; j <= nodes; j++)
			printf("%d ", dist[i][j]);
		printf("\n");
	}
	printf("--------------------------------------------------------------------------------\n");
	return 0;
}



