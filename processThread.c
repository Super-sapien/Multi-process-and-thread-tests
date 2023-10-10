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
#define MAX_CHILDREN 3
static int numChildren = 0;

typedef double MathFunc_t(double);

typedef struct {
    MathFunc_t *func;
    double rangeStart;
    double rangeEnd;
	size_t numSteps;
    double *total;

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
// pass in worker to integrateTrap
void* integrateTrap(void *ptr)
{
    Worker *worker = (Worker*)ptr;
	double rangeSize = worker->rangeEnd - worker->rangeStart;
	double dx = rangeSize / worker->numSteps;

	double area = 0;
	for (size_t i = 0; i < worker->numSteps; i++) {
		double smallx = worker->rangeStart + i*dx;
		double bigx = worker->rangeStart + (i+1)*dx;

		area += dx * ( worker->func(smallx) + worker->func(bigx) ) / 2; //Would be more efficient to multiply area by dx once at the end. 
	}
	
	// area *= dx;
    // do the mutex locking and unlocking while changing total
    pthread_mutex_lock(worker->lock); // lock other processes out of critical region
    (*worker->total) += area; // increment the total
    pthread_mutex_unlock(worker->lock); // unlock so other processes can access if needed
    // pthread_exit(NULL);
	return NULL;
}



bool getValidInput(double* start, double* end, size_t* numSteps, size_t* funcId)
{
	printf("Query: [start] [end] [numSteps] [funcId]\n");

	//Read input numbers and place them in the given addresses:
	size_t numRead = scanf("%lf %lf %zu %zu", start, end, numSteps, funcId);

	//Return whether the given range is valid:
	return (numRead == 4 && *end >= *start && *numSteps > 0 && *funcId < NUM_FUNCS);
}

void childDied() {
	numChildren--;
}


int main(void)
{
	double rangeStart;
	double rangeEnd;
	size_t numSteps;
	size_t funcId;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // initialize mutex
    Worker workers[MAX_THREADS];
    
	pid_t childPid;
	signal(SIGCHLD, childDied);  /* register signal for when child stopped or terminated */
    while (1) { // parent reprompts for input by calling getValidInput
		if (numChildren < MAX_CHILDREN) {
			if (getValidInput(&rangeStart, &rangeEnd, &numSteps, &funcId)) {
				childPid = fork();
				if (childPid == 0) { // inside child process
                	double total = 0;
                    double rangePerStep = (rangeEnd - rangeStart) / MAX_THREADS;
                    int stepCount = 0;
     
                    for (int i = 0; i < MAX_THREADS; ++i) {
                        Worker *worker = &workers[i]; // Create workers
                        worker->total = &total; // Pass the global total into each thread
                        worker->lock = &lock; // init the workers mutex
                        worker->func = FUNCS[funcId];
                        
                        worker->rangeStart = rangeStart + i * rangePerStep;
                        worker->rangeEnd = rangeStart + (i + 1) * rangePerStep;

                        if (i == MAX_THREADS - 1) {
                            worker->numSteps = numSteps - stepCount;
                        } else {
                            worker->numSteps = floor(numSteps / MAX_THREADS);
                            stepCount += floor(numSteps / MAX_THREADS);
                        }
                        pthread_create(&worker->thread, NULL, integrateTrap, (void*)worker);
                    }

                    for (int i = 0; i < MAX_THREADS; ++i) {
                        pthread_join(workers[i].thread, NULL); // wait for each thread to terminate
                    }

                    printf("The integral of function %zu in range %g to %g is %.10g\n", funcId, rangeStart, rangeEnd, total);
                // }
                    // double area = integrateTrap(FUNCS[funcId], rangeStart, rangeEnd, numSteps); // child runs integrateTrap
                    // printf("The integral of function %zu in range %g to %g is %.10g\n", funcId, rangeStart, rangeEnd, area);
                    // fflush(stdout);
                    // _exit(0); // forces exit immediately
                } else {
                    numChildren++;
                }
            } else {
                break;
            }
        };
    }
    while (wait(NULL) > 0) {
        continue;
    }
    
	exit(0);
}