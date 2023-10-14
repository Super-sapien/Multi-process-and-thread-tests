#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_THREADS 32
#define MAX_CHILDREN 32
static int numChildren = 0;

typedef double MathFunc_t(double);

typedef struct {
    MathFunc_t *func;
    double rangeStart;
    double rangeEnd;
	size_t numSteps;
    double *totalArea;
	size_t startIndex;
    pthread_mutex_t *lock;
    pthread_t thread;
} Worker;


double gaussian(double x)
{
	return exp(-(x*x)/2) / (sqrt(2 * M_PI));
}


double chargeDecay(double x)
{
	if (x < 0) {
		return 0;
	} else if (x < 1) {
		return 1 - exp(-5*x);
	} else {
		return exp(-(x-1));
	}
}

#define NUM_FUNCS 3
static MathFunc_t* const FUNCS[NUM_FUNCS] = {&sin, &gaussian, &chargeDecay};



//Integrate using the trapezoid method. 
void* integrateTrap(void *ptr)
{
    Worker *worker = (Worker*)ptr;
	double rangeSize = worker->rangeEnd - worker->rangeStart;
	double dx = rangeSize / worker->numSteps;

	double area = 0;
	for (size_t i = worker->startIndex; i < worker->numSteps; i += MAX_THREADS) {
		double smallx = worker->rangeStart + i*dx;
		double bigx = worker->rangeStart + (i+1)*dx;

		area += dx * ( worker->func(smallx) + worker->func(bigx) ) / 2; //Would be more efficient to multiply area by dx once at the end. 
	}
	
	// area *= dx;
    pthread_mutex_lock(worker->lock); // lock other processes out of critical region
    (*worker->totalArea) += area;
    pthread_mutex_unlock(worker->lock); // unlock so other processes can access if needed
	return NULL;
}


bool getValidInput(double* start, double* end, size_t* numSteps, size_t* funcId)
{
	// printf("Query: [start] [end] [numSteps] [funcId]\n");
	// fflush(stdout); // immediately write out the stdout buffer (prevents print stmt being written more than desired)
	//Read input numbers and place them in the given addresses:
	size_t numRead = scanf("%lf %lf %zu %zu ", start, end, numSteps, funcId);

	//Return whether the given range is valid:
	return (numRead == 4 && *end >= *start && *numSteps > 0 && *funcId < NUM_FUNCS);
}


void childDied() {
	numChildren--;
}

void spawnProcessThread(size_t funcId, double rangeStart, double rangeEnd, size_t numSteps) {
    pid_t childPid = fork();
    if (childPid == 0) { // inside child process
        double totalArea = 0;
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // initialize mutex
        Worker workers[MAX_THREADS];

        // Create workers
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            Worker *worker = &workers[i]; 
            worker->totalArea = &totalArea;
            worker->lock = &lock;
            worker->func = FUNCS[funcId];
            worker->startIndex = i;
            worker->rangeStart = rangeStart;
            worker->rangeEnd = rangeEnd;
            worker->numSteps = numSteps;
            
            pthread_create(&worker->thread, NULL, integrateTrap, (void*)worker);
        }

        for (size_t i = 0; i < MAX_THREADS; ++i) {
            pthread_join(workers[i].thread, NULL); // wait for each thread to terminate and join results
        }
        printf("The integral of function %zu in range %g to %g is %.10g\n", funcId, rangeStart, rangeEnd, totalArea);
        fflush(stdout); // immediately print output buffer
        _exit(0); // forces exit from child process immediately

        } else { // in parent
        numChildren++;
    }
}


int main(void)
{
	double rangeStart;
	double rangeEnd;
	size_t numSteps;
	size_t funcId;

	signal(SIGCHLD, childDied);  /* register signal for when child stopped or terminated */
    while (1) { // parent loops while getValidInput returns valid input true
		if (numChildren < MAX_CHILDREN) { // 
			if (getValidInput(&rangeStart, &rangeEnd, &numSteps, &funcId)) {
				spawnProcessThread(funcId, rangeStart, rangeEnd, numSteps);
            } else { // no more input, end loop
                break;
            }
        };
    }
    // Parent waits for all children to have returned
    while (wait(NULL) > 0) {
        continue;
    }

	exit(0);
}