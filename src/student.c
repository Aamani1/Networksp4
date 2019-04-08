
/*
 * student.c
 * Multithreaded OS Simulation for CS 2200
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "os-sim.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;

enum {FIFO, ROUND_ROBIN, SRTF} algorithm = FIFO;

static void push(pcb_t* readyQueue) {
    pthread_mutex_lock(&queue_mutex);
    /* FIFO or ROUND-ROBIN */
    if (algorithm != SRTF) {
        if (head == NULL) {
            head = readyQueue;
            tail = readyQueue;
            head->next = NULL;
            tail->next = NULL;
        } else {
            tail->next = readyQueue;
            tail = readyQueue;
        }
    } 
    else /* SRTF */
    {
        if (head == NULL || (head->time_remaining >= readyQueue->time_remaining)) {
            readyQueue->next = head;
            head = readyQueue;
        } else {
            pcb_t* curr_temp;
            curr_temp = head;

            while (curr_temp->next != NULL && (readyQueue->time_remaining > curr_temp->next->time_remaining)) {
                curr_temp = curr_temp->next;
            }
            readyQueue->next = curr_temp->next;
            curr_temp->next = readyQueue;
        }
    }
    pthread_cond_signal(&signal);
    pthread_mutex_unlock(&queue_mutex);    
}    

static pcb_t* pop() {
    pthread_mutex_lock(&queue_mutex);
    pcb_t* popReadyQueue = head;
    if (popReadyQueue != NULL) {
       head = head->next;
       popReadyQueue->next = NULL;
    } 
    if (head == NULL) {
        tail = NULL;
    }
    pthread_mutex_unlock(&queue_mutex);
    return popReadyQueue;
}

/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Set the currently running process using the current array
 *
 *   4. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *
 *	The current array (see above) is how you access the currently running process indexed by the cpu id. 
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
    /* FIX ME */
    pcb_t* removeNode = pop();
    if ((removeNode != NULL)) {
        removeNode->state =  PROCESS_RUNNING;
    } 
    pthread_mutex_lock(&current_mutex);
    current[cpu_id] = removeNode;
    pthread_mutex_unlock(&current_mutex);
    context_switch(cpu_id, removeNode, timeSlice);
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
    /* FIX ME */
    // schedule(0);
    pthread_mutex_lock(&queue_mutex);
    while (head == NULL) {
        pthread_cond_wait(&signal, &queue_mutex); 
    }
    pthread_mutex_unlock(&queue_mutex);
    schedule(cpu_id);


    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
    // mt_safe_usleep(1000000);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 *
 * Remember to set the status of the process to the proper value.
 */
extern void preempt(unsigned int cpu_id)
{
    /* FIX ME */
    pthread_mutex_lock(&current_mutex);
    push(current[cpu_id]);
    current[cpu_id]->state = PROCESS_READY;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    /* FIX ME */
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_WAITING;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
    /* FIX ME */
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is Priority, wake_up() may need
 *      to preempt the CPU with the lowest priority to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with higher priority than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for
 * 	its prototype and the parameters it takes in.
 *
 *  NOTE: A high priority corresponds to a low number.
 *  i.e. 0 is the highest possible priority.
 */
extern void wake_up(pcb_t *process)
{
    /* FIX ME */
    process->state = PROCESS_READY;
    push(process);
    if (algorithm == SRTF) {
        unsigned int id = 0;
        pthread_mutex_lock(&current_mutex);
        for (unsigned int i = 0; i < cpu_count_temp; i++) {
            if (current[i] == NULL || current[id] == NULL) {
                pthread_mutex_unlock(&current_mutex);
                return;
            }
            if (current[id]->time_remaining > current[i]->time_remaining) {
                id = i;
            }
        }

        if (current[id]->time_remaining > process->time_remaining) {
            pthread_mutex_unlock(&current_mutex);
            force_preempt(id);
        }
        pthread_mutex_unlock(&current_mutex);
    }
}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */
int main(int argc, char *argv[])
{
    unsigned int cpu_count;

    /*
     * Check here if the number of arguments provided is valid.
     * You will need to modify this when you add more arguments.
     */
    if (argc != 2)
    {
        fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n"
            "         -p : Priority Scheduler\n\n");
        return -1;
    }

    /* Parse the command line arguments */
    cpu_count = strtoul(argv[1], NULL, 0);
    cpu_count_temp = cpu_count;

    /* FIX ME - Add support for -r and -p parameters*/
    int option = getopt(argc, argv, "rs");
    switch(option) {
        case 'r':
            algorithm = ROUND_ROBIN;
            timeSlice  = atoi(argv[optind]);
            break;
        case 's' :
            algorithm = SRTF;
            timeSlice  = -1;
            break;
        default :
            algorithm = FIFO;  
            timeSlice  = -1; 

    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&signal, NULL);
    /* Start the simulator in the library */
    start_simulator(cpu_count);
    return 0;
}


#pragma GCC diagnostic pop
