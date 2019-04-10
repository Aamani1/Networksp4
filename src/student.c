
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
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);


void help(void);


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


static int time_slice;
static pthread_cond_t no_idle;
static pcb_t* queue_head;
static int strf_true;
static unsigned int cpu_count;
static pthread_mutex_t rq_mutex;
static int round_robin;
static int prior;



static void add_queue(pcb_t* pcb)
{

    pthread_mutex_lock(&rq_mutex);

    pcb_t *curr_pcb = queue_head;

    pcb->next = NULL;

    if (curr_pcb != NULL) {

        while (curr_pcb->next != NULL) {
            curr_pcb = curr_pcb->next;
        }

        curr_pcb->next = pcb;
    } else {
        queue_head = pcb;
    }

    pthread_cond_broadcast(&no_idle);
    pthread_mutex_unlock(&rq_mutex);
}

static pcb_t* pop_queue()
{

    pcb_t* pop_pcb;
    pthread_mutex_lock(&rq_mutex);

    pop_pcb = queue_head;

    if (pop_pcb != NULL)
    {
        queue_head = pop_pcb->next;
    }

    pthread_mutex_unlock(&rq_mutex);
    return pop_pcb;
}


static pcb_t* priority_queue() {

    pcb_t *curr;
    pcb_t *highest;
    curr = queue_head;
    unsigned int level = 0;

    if (queue_head == NULL) {
        return NULL;
    }

    if(queue_head->next == NULL){
        queue_head = NULL;
        return curr;
    } else {
        while (curr != NULL) {
            if (curr->priority > level) {
                level = curr->priority;
                highest = curr;
            }
            curr = curr->next;
        }
        curr = queue_head;
        if (highest == queue_head) {
            queue_head = queue_head->next;
            return highest;
        }

        while (curr->next != highest) {
            curr = curr->next;
        }
        curr->next = highest->next;
    }
return highest;
}

void help()
{
    fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n"
            "         -p : Priority Scheduler\n\n");
}


/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which
 *   you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Set the currently running process using the current array
 *
 *   4. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *
 *   The current array (see above) is how you access the currently running process indexed by the cpu id.
 *   See above for full description.
 *   context_switch() is prototyped in os-sim.h. Look there for more information
 *   about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
    pcb_t *pcb_process;

    if (prior == 1)
    {
        pcb_process = priority_queue();
    }
    else
    {
        pcb_process = pop_queue();
    }

    if (pcb_process != NULL)
    {
        pcb_process->state = PROCESS_RUNNING;
    }

    pthread_mutex_lock(&current_mutex);
    current[cpu_id] = pcb_process;

    pthread_mutex_unlock(&current_mutex);
    context_switch(cpu_id, pcb_process, time_slice);
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

    pthread_mutex_lock(&rq_mutex);
    while (queue_head == NULL)
    {
        pthread_cond_wait(&no_idle, &rq_mutex);
    }

    pthread_mutex_unlock(&rq_mutex);
    schedule(cpu_id);
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
    pthread_mutex_lock(&current_mutex);
    pcb_t* pcb_preempt = current[cpu_id];
    pcb_preempt->state = PROCESS_READY;
    pthread_mutex_unlock(&current_mutex);

    add_queue(pcb_preempt);
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

    pthread_mutex_lock(&current_mutex);
    pcb_t *pcb_yield;
    pcb_yield = current[cpu_id];

    pcb_yield->state = PROCESS_WAITING;
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
    pthread_mutex_lock(&current_mutex);
    pcb_t* pcb_terminate;
    pcb_terminate = current[cpu_id];

    pcb_terminate->state = PROCESS_TERMINATED;
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
 *  To preempt a process, use force_preempt(). Look in os-sim.h for
 *  its prototype and the parameters it takes in.
 *
 *  NOTE: A high priority corresponds to a low number.
 *  i.e. 0 is the highest possible priority.
 */
extern void wake_up(pcb_t *process)
{

    int lowest = 10, run_pcb, cont = 0;

    process->state = PROCESS_READY;
    add_queue(process);

    if (prior == 1)
    { pthread_mutex_lock(&current_mutex);

        for (int i = 0; i < cpu_count; i++)
        {
            if (current[i]==NULL)
            {
                cont = 1;
                break;

            }
            if (current[i]->priority < lowest)
            {
              lowest = current[i]->priority;
              run_pcb = i;
            }
        }
        pthread_mutex_unlock(&current_mutex);

        if (cont !=1 && lowest<process->priority )
        {
           force_preempt(run_pcb);
        }
    }

}



/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -s command-line parameters.
 */
int main(int argc, char *argv[])
{

    strf_true = 0;
    round_robin = 0;
    prior = 0;
    time_slice = -1;

    if (argc > 4|| argc < 2)
    {
        fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n"
            "         -p : Priority Scheduler\n\n");
        return -1;
    }

    cpu_count = strtoul(argv[1], NULL, 0);

    if (cpu_count == 0)
    {
        help();
        return -1;
    }

    if (argc > 2) {

        if (strcmp(argv[2], "-p") == 0)
        {
             prior = 1;
        }

        if (strcmp(argv[2],"-r") == 0)
        {
            round_robin = 1;
        }

        if (argc > 3)
        {
            time_slice = strtoul(argv[3], NULL, 0);
        }

    }

    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);

    pthread_mutex_init(&rq_mutex, NULL);
    queue_head = NULL;
    pthread_cond_init(&no_idle, NULL);

    start_simulator(cpu_count);

    return 0;
}


#pragma GCC diagnostic pop