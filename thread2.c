#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

#define NUM_THREADS 32

typedef double MathFunc_t(double);

typedef struct {
    double *totalArea;
    double rangeStart;
    double rangeEnd;
    size_t numSteps;
    MathFunc_t *func;

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
void *integrateTrap(void *ptr)
{
    // Unpack the variables from the worker object
    Worker *worker = (Worker*)ptr;

    double rangeStart = worker->rangeStart;
    double rangeEnd = worker->rangeEnd;
    size_t numSteps = worker->numSteps;
    MathFunc_t* func = worker->func;

	double rangeSize = rangeEnd - rangeStart;
	double dx = rangeSize / numSteps;

	double area = 0;
	for (size_t i = 0; i < numSteps; i++) {
		double smallx = rangeStart + i*dx;
		double bigx = rangeStart + (i+1)*dx;

		area += dx * ( func(smallx) + func(bigx) ) / 2; //Would be more efficient to multiply area by dx once at the end. 
	}

    // Add to the total area variable accessed by all threads in this process
    // Uses a mutex to stop multiple threads trying to write at the same time
    pthread_mutex_lock(worker->lock);
    (*worker->totalArea) += area;
    pthread_mutex_unlock(worker->lock);

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



int main(void)
{
	double rangeStart;
	double rangeEnd;
	size_t numSteps;
	size_t funcId;


    // Create a mutex lock and a list of workers
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    Worker workers[NUM_THREADS];

	while (getValidInput(&rangeStart, &rangeEnd, &numSteps, &funcId)) {
        // Variables for calculating what each thread should process
        double totalArea = 0;
        int stepsDone = 0;
        double threadRange = (rangeEnd - rangeStart) / NUM_THREADS;

        for (int i = 0; i < NUM_THREADS; i++) {
            // Create a worker object and store it
            Worker *worker = &workers[i];

            // Add area and mutex variables to the worker
            worker->totalArea = &totalArea;
            worker->lock = &lock;

            // Calculate the range of the area to integrate for the thread
            worker->rangeStart = rangeStart + i * threadRange;
            worker->rangeEnd = rangeStart + (i + 1) * threadRange;

            // Calculate the number of steps to do for the thread.
            // Use floor division to calculate for all but the last thread
            // The last thread does as many steps as needed to take it up
            // to the total number of steps required
            if (i == NUM_THREADS - 1) {
                worker->numSteps = numSteps - stepsDone;
            } else {
                worker->numSteps = floor(numSteps / NUM_THREADS);
                stepsDone += floor(numSteps / NUM_THREADS);
            }

            worker->func = FUNCS[funcId];

            // Create the thread and tell it to integrate with the variables in the worker object
            pthread_create(&worker->thread, NULL, integrateTrap, (void*)worker);
        }

        // Wait for all threads to be done
        for (int i = 0; i < NUM_THREADS; i++) {
            void *ret;
            Worker *worker = &workers[i];
            pthread_join(worker->thread, &ret);
        }

		printf("The integral of function %zu in range %g to %g is %.10g\n", funcId, rangeStart, rangeEnd, totalArea);
	}

	exit(0);
}