#define _GNU_SOURCE 1
#define __USE_GNU 1
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "func.h"
#include "mytime.h"

#define rdtsc(time)                 \
__asm__ __volatile__ (           \
"rdtsc;\n\t"                  \
"shlq $32, %%rdx;\n\t"        \
"addq %%rdx, %%rax;\n\t"      \
: "=a"(time) : : "%rdx" );

//this first global int are where the individual threads
//add their results from testing
//the second is iterated everytime a thread completes their work 
//so the master knows when to continue with adding new work or closing threads
//the third is a counter that keeps track of how many threads are asleep

static pthread_mutex_t wqlock;

//this program utilizes a work queue to deliver work orders to the threads
//which constantly poll for more work

struct workq
{
  struct workq *next;
  //this is a meta value that tells the thread what type of work will be done.
  //for now, 1 means perform montecarlo integration
  //while 0 means kill the thread.
  int workOrder;
  int virtualrank;
} *workhead;
struct threadstruct
{
  volatile int threadrank;
  volatile int *threadarray;
};
//function shows user what flags do what in the program

void PrintUsage(char *name, int iarg, char *arg)
{
  fprintf(stderr, "\nERROR around arg %d (%s).\n", iarg, arg ? arg:"unknown");
  fprintf(stderr, "USAGE: %s [flags], where flags are:\n", name);
  fprintf(stderr, "   -C <#> : set clockspeed VERY IMPORTANT FOR TIMINGS to #\n");
  fprintf(stderr, "   -p <#> : set number of threads to #\n");
  fprintf(stderr, "   -c <#> : set number of cores to #\n");
  fprintf(stderr, "   -t <#> : set number of tests to #\n");
  exit(iarg ? iarg : -1);
}

//this function retrieves all flags needed for the program

void GetFlags(int nargs, char **args, double *clk, int *p, int *num_cpus, int *tst_num)
{
  int i;
  
  //the following values are the default flags and will be used
  //if a flag is not explicitly given
  
  *clk = 2400;
  *p = 4;
  *num_cpus = 4;
  *tst_num = 50;
  
  for (i=1; i < nargs; i++)
  {
    if(args[i][0] != '-')
      PrintUsage(args[0], i, args[i]);
    switch(args[i][1])
    {
      case 'C':
	if (++i >= nargs)
	  PrintUsage(args[0], i, "out of arguments");
	*clk = atoi(args[i]);
	break;
      case 'p':
	if (++i >= nargs)
	  PrintUsage(args[0], i, "out of arguments");
	if(atoi(args[i]) == 0)
	  break;
	*p = atoi(args[i]);
	break;
      case 'c':
	if (++i >= nargs)
	  PrintUsage(args[0], i, "out of arguments");
	if(atoi(args[i]) == 0)
	  break;
	*num_cpus = atoi(args[i]);
	break;
      case 't':
	if (++i >= nargs)
	  PrintUsage(args[0], i, "out of arguments");
	if(atoi(args[i]) == 0)
	  break;
	*tst_num = atoi(args[i]);
	break;
      default:
	PrintUsage(args[0], i, args[i]);
    }
  }
}

void LocalMonteCarlo()
{
  
}

void *thread(void *thrdstructure)
{
  struct workq *mywork;
  struct threadstruct *mystruct;
  mystruct = thrdstructure;
  //here we create an infinite loop for the thread so that it can 
  //constantly poll for the next work order
  while(1)
  {
    if(workhead)
    {
      //we need a lock here to prevent two threads competing 
      //for the same work order
      pthread_mutex_lock(&wqlock);
      //we need to check again after passing the lock for the 
      //work head in case the previous thread took the last work order
      if(workhead)
      {
	//if there is still a workorder left,
	//remove it from the queue and unlock the threads
	mywork = workhead;
	workhead = workhead->next;
	pthread_mutex_unlock(&wqlock);
	
	//workorder 0 has the thread exit after incrementing  
	//hitcounter to notify the master it has completed
	//work
	if(mywork->workOrder == 0)
	{
	  //lock around inrementation of hitcounter
	  //to prevent race condition
	  mystruct->threadarray[mywork->virtualrank] = 1;
	  pthread_exit(NULL);
	}                
	//we need to locks around these variables since they are 
	//global and we want to prevent race conditions	
	
	mystruct->threadarray[mywork->virtualrank] = 1;
      }
      
      //if the last work order was already taken, unlock 
      //and return to the top of the loop
      else
      {
	pthread_mutex_unlock(&wqlock);
	continue;
      }
    }
  }
  return(NULL);
}

//this function adds work orders into the queue.
//mutex lock needed so a work order isnt taken while adding
void AddWork(struct workq *p)
{
  pthread_mutex_lock(&wqlock);
  p->next = workhead;
  workhead = p;
  pthread_mutex_unlock(&wqlock);
}

int main(int nargs, char **args)
{
  
  int n, i, p, seed, num_cpus, tst_num, j, k, l , m;
  unsigned long time1, time2;
  double bst_strt, wrst_strt, avg_strt, bst_proc, wrst_proc, avg_proc, bst_kill, wrst_kill, avg_kill, total_time, avg_total, bst_total, wrst_total, start_time, process_time, kill_time;
  avg_kill = 0;
  avg_proc = 0;
  avg_strt = 0;
  avg_total = 0;
  //we start cpu_ctr at 1 so that threads start spawning 
  //on the second core since the master thread is already tied down 
  //from taskset
  int cpu_ctr = 1; 
  double clockspeed;
  unsigned int N, n_extra;
  struct workq *mywork;
  pthread_attr_t attr;
  cpu_set_t cpuset;
  pthread_t *mythrs;
  volatile int *hitarray;
  
  struct threadstruct *thrdstruct;
  
  
  //retrieve flags specified by person running program
  GetFlags(nargs, args, &clockspeed, &p, &num_cpus, &tst_num);
  //initializes pthread attr's for locking down threads to cores and 
  //init's the mutex lock
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  pthread_mutex_init(&wqlock, NULL);
  
  mythrs = malloc(sizeof(pthread_t)*(p));
  hitarray = malloc(sizeof(int)*(p));
  thrdstruct = malloc(sizeof(struct threadstruct)*(p));
  mywork = malloc(sizeof(struct workq)*(p));
  for(i=0;i<p;i++)
  {
    hitarray[i] = 0;
  }
  //this creates p-1 threads for the program, we dont need 
  //p number threads created since the one we started the program 
  //with is being used for work aswell
  for(i=0;i<tst_num;i++)
  {
    rdtsc(time1);
    for(k=1;k<p;k++)
    {
      //this checks if the core we are trying to create a thread on exists
      //if it does not, we roll back the core counter to the first core
      if(cpu_ctr == num_cpus)
	cpu_ctr = 0;
      //this clears our cpu set
      CPU_ZERO(&cpuset);
      //this adds the core we want to add a pthread to into the cpu set
      CPU_SET(cpu_ctr, &cpuset);
      
      (thrdstruct+k)->threadarray = hitarray;
      (thrdstruct+k)->threadrank = k;
      //this makes the core we wish to make a pthread on one of 
      //our pthread's attr's 
      pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);
      //by making our pthread asynchronous it can be exited at 
      //any time in its execution
      pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
      pthread_create(mythrs+k, &attr, thread, (thrdstruct+k));
      //move to the next core
      cpu_ctr++;
    }
    
    rdtsc(time2);
    start_time = (time2 - time1) / (clockspeed * 1000000);
    rdtsc(time1);
    for(j=1;j<p;j++)
    {
      //tells the thread this is a monte carlo work order
      (mywork+j)->workOrder = 1;
      (mywork+j)->virtualrank = j;
      //add into work queue
      AddWork((mywork+j));
    }
    hitarray[0] = 1;
    //master thread will pause here until all worker threads 
    //have completed work
    j = 0;
    while(1)
    {
      if(j<p)
      {
	if(hitarray[j] == 1)
	{
	  hitarray[j] = 0;
	  j++;
	}
      }
      else
	break;
    }
    rdtsc(time2);

    process_time = (time2 - time1) / (clockspeed * 1000000);
    rdtsc(time1);
    
    for(l=1;l<p;l++)
    {
      (mywork+j)->workOrder = 0;
      (mywork+j)->virtualrank = j;
      AddWork((mywork+l));
    }
    //this loop checks for all threads to be completed so that we dont try 
    //freeing memory that is in use at the end
    j = 0;
    hitarray[0] = 1;
    while(1)
    {
      
      if(j<p)
      {
	if(hitarray[j] == 1)
	{
	  hitarray[j] = 0;
	  j++;
	}
      }
      else
	break;
    }
    rdtsc(time2);
    kill_time = (time2 - time1) / (clockspeed * 1000000);
    total_time = start_time + kill_time + process_time;
    avg_strt += start_time;
    avg_proc += process_time;
    avg_kill += kill_time;
    avg_total += total_time;
    if(i == 0)
    {
      bst_strt = start_time;
      bst_proc = process_time;
      bst_kill = kill_time;
      wrst_strt = start_time;
      wrst_proc = process_time;
      wrst_kill = kill_time;
      bst_total = total_time;
      wrst_total = total_time;
    }
    else
    {
      if(bst_strt > start_time)
	bst_strt = start_time;
      if(wrst_strt < start_time)
	wrst_strt = start_time;
      if(bst_proc > process_time)
	bst_proc = process_time;
      if(wrst_proc < process_time)
	wrst_proc = process_time;
      if(bst_kill > kill_time)
	bst_kill = kill_time;
      if(wrst_kill < kill_time)
	wrst_strt = start_time;
      if(bst_total > total_time)
	bst_total = total_time;
      if(wrst_total < total_time)
	wrst_total = total_time;      
    }
    fprintf(stdout, "start time = %f , process time = %f , kill time = %f\n", start_time, process_time, kill_time);
    fprintf(stdout, "total time = %f\n", total_time);
  }
  fprintf(stdout, "best start time = %f , worst start time = %f , average start time = %f\n", bst_strt, wrst_strt, avg_strt / tst_num);
  fprintf(stdout, "best process time = %f , worst process time = %f , average process time = %f\n", bst_proc, wrst_proc, avg_proc / tst_num);
  fprintf(stdout, "best kill time = %f , worst kill time = %f , average kill time = %f\n", bst_kill, wrst_kill, avg_kill / tst_num);
  fprintf(stdout, "best total time = %f , worst total time = %f , average total time = %f\n",bst_total, wrst_total, avg_total / tst_num);
  //free(mythrs);
}