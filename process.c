#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

/* the more threads you have, the faster it runs, the less accurate it is? */
#define MAX_CHILDREN 3
static int numChildren = 0;

typedef double MathFunc_t(double);


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
double integrateTrap(MathFunc_t* func, double rangeStart, double rangeEnd, size_t numSteps)
{
	double rangeSize = rangeEnd - rangeStart;
	double dx = rangeSize / numSteps;

	double area = 0;
	for (size_t i = 0; i < numSteps; i++) {
		double smallx = rangeStart + i*dx;
		double bigx = rangeStart + (i+1)*dx;

		area += dx * ( func(smallx) + func(bigx) ) / 2; //Would be more efficient to multiply area by dx once at the end. 
	}

	return area;
}




bool getValidInput(double* start, double* end, size_t* numSteps, size_t* funcId)
{
	printf("Query: [start] [end] [numSteps] [funcId]\n");

	//Read input numbers and place them in the given addresses:
	size_t numRead = scanf("%lf %lf %zu %zu ", start, end, numSteps, funcId);

	//Return whether the given range is valid:
	return (numRead == 4 && *end >= *start && *numSteps > 0 && *funcId < NUM_FUNCS);
}

void childDied() {
	numChildren--;
	// signal(SIGCHLD, childDied);
	// wait(NULL);
}

int main(void)
{
	double rangeStart;
	double rangeEnd;
	size_t numSteps;
	size_t funcId;

	pid_t childPid;
	signal(SIGCHLD, childDied);  /* register signal for when child stopped or terminated */
	while (1) { // parent reprompts for input by calling getValidInput
		/* wait here for num children to be less than max children */
		/* TODO: change numChildren to a semaphore */
		// while (numChildren >= MAX_CHILDREN) {
		// 	wait(NULL);
		// }
		if (numChildren < MAX_CHILDREN) {
			if (getValidInput(&rangeStart, &rangeEnd, &numSteps, &funcId)) {
				childPid = fork();
				if (childPid == 0) { // inside child process
					double area = integrateTrap(FUNCS[funcId], rangeStart, rangeEnd, numSteps); // child runs integrateTrap
					printf("The integral of function %zu in range %g to %g is %.10g\n", funcId, rangeStart, rangeEnd, area);
					fflush(stdout);
					_exit(0); // forces exit immediately
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
	// while (numChildren > 0) {
	// 	pause();
	// }

	exit(0);
}