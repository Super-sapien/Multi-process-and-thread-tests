#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#define NUM_FUNCS 3	   // number of math functions
#define NUM_CHILDREN 3 // number of children that can be created

// declare a macro to handle exceptions
#define error_exit(prompt)  \
	do                      \
	{                       \
		perror(prompt);     \
		exit(EXIT_FAILURE); \
	} while (0)

// declare a macro to handle child exceptions
#define error_child_exit(prompt) \
	do                           \
	{                            \
		perror(prompt);          \
		_exit(EXIT_FAILURE);     \
	} while (0)

// define the signatures of the math functions
double gaussian(double);
double charge_decay(double);

// declare an array of math functions
typedef double math_func_t(double);
static math_func_t *const funcs[NUM_FUNCS] = {&sin, &gaussian, &charge_decay};

// decalre a pipe to "self-pipe" to parent process
static int self_pipe_fds[2] = {0};

// gaussian function
double gaussian(double x)
{
	return exp(-(x * x) / 2) / (sqrt(2 * M_PI));
}

// charge decay function
double charge_decay(double x)
{
	if (x < 0)
		return 0;
	else if (x < 1)
		return 1 - exp(-5 * x);
	else
		return exp(-(x - 1));
}

// integrate using the trapezoid method.
double integrate_trap(math_func_t *func, double range_start, double range_end, size_t num_steps)
{
	double range_size = range_end - range_start; // the size of the integral
	double dx = range_size / num_steps;			 // the integral increment amount

	double area = 0;					   // the area of the integral
	for (size_t i = 0; i < num_steps; i++) // iterate over all steps in the integral
	{
		double smallx = range_start + i * dx;	  // the left-x value
		double bigx = range_start + (i + 1) * dx; // the right-x value

		area += func(smallx) + func(bigx); // the sum of the function at the x values
	}

	area = dx * area / 2; // finish the area calculation

	return area; // return the total area
}

// get a valid input from stdin
bool get_valid_input(double *start, double *end, size_t *num_steps, size_t *func_id)
{
	// print the query
	if (printf("Query: [start] [end] [num_steps] [func_id]\n") < 0)
		error_exit("printf");

	size_t num_read = scanf("%lf %lf %zu %zu", start, end, num_steps, func_id); // read from stdin and grab the values

	return (num_read == 4 && *end >= *start && *num_steps > 0 && *func_id < NUM_FUNCS); // sanity check
}

// spawn a child process to handle the integration
void spawn_child_process(double range_start, double range_end, size_t num_steps, size_t func_id)
{
	int pid = fork(); // create a child process
	if (pid == -1)
		error_exit("fork");
	else if (pid != 0) // parent process does not belong here
		return;

	// close the pipes in the child processes as theyre only for the parent
	if (close(self_pipe_fds[0]) == -1)
		error_child_exit("close");
	if (close(self_pipe_fds[1]) == -1)
		error_child_exit("close");

	double area = integrate_trap(funcs[func_id], range_start, range_end, num_steps); // do the integration

	// print the result
	if (printf("The integral of function %zu in range %g to %g is %.10g\n", func_id, range_start, range_end, area) < 0)
		error_child_exit("printf");

	_exit(EXIT_SUCCESS); // finished
}

// asynchronously wait for a child process to exit
void async_child_wait(int signum)
{
	signal(SIGCHLD, async_child_wait); // re-instantiate the signal

	wait(NULL); // acknowledge/wait for the child's death

	// write one byte to the same process (non-blocking)
	write(self_pipe_fds[1], ".", 1); // we cant really error check this
}

// created pipes to communicate between signal handler and poll
void init_self_poll(void)
{
	if (pipe(self_pipe_fds) == -1) // create a pipe
		error_exit("pipe");

	/*
		we do not care about the other flags on linux when calling fcntl
		i.e. O_APPEND, O_ASYNC, O_DIRECT, and O_NOATIME
	*/
	if (fcntl(self_pipe_fds[0], F_SETFL, O_NONBLOCK) == -1) // set the output end as non-blocking
		error_exit("fcntl_setfl");

	if (fcntl(self_pipe_fds[1], F_SETFL, O_NONBLOCK) == -1) // set the input end as non-blocking
		error_exit("fcntl_setfl");

	char data[NUM_CHILDREN] = {0};						   // create a byte array for NUM_CHILDREN bytes
	if (write(self_pipe_fds[1], data, NUM_CHILDREN) == -1) // fill pipe with NUM_CHILDREN bytes
		error_exit("write");
}

// uses polling to wait for a process to be available
bool self_poll(void)
{
	// create a poll type variable to read from a fd
	struct pollfd pollfd = {0};
	pollfd.fd = self_pipe_fds[0];
	pollfd.events = POLLIN;

	// store the return value of the poll
	int poll_ret;
	while ((poll_ret = poll(&pollfd, 1, -1)) == -1 && errno == EINTR) // continue polling if poll was interrupted
		;

	if (poll_ret == -1) // if something else went wrong
		error_exit("poll");

	// read the byte from the buffer (non-blocking)
	char discard;
	if (read(pollfd.fd, &discard, 1) == -1)
		error_exit("read");

	return true; // finished
}

// entry point of the program
int main(void)
{
	// define variables populated by stdin
	double range_start, range_end;
	size_t num_steps, func_id;

	init_self_poll(); // init file descriptors for self-polling

	if (signal(SIGCHLD, async_child_wait) == SIG_ERR) // create a signal to listen for child process's exiting
		error_exit("signal");

	// wait for a child process to be available and check for valid input
	while (self_poll() && get_valid_input(&range_start, &range_end, &num_steps, &func_id))
		spawn_child_process(range_start, range_end, num_steps, func_id); // spawn a child process

	while (wait(NULL) != -1) // (safety) wait for all child processes to finish (if any)
		;

	// close the pipes
	if (close(self_pipe_fds[0]) == -1)
		error_exit("close");
	if (close(self_pipe_fds[1]) == -1)
		error_exit("close");

	exit(EXIT_SUCCESS); // finished
}