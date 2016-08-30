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
  volatile double *threadarray;
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
  unsigned long time;
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
	rdtsc(time);
	mystruct->threadarray[mywork->virtualrank] = time;
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
int sort(const void *x, const void *y)
{
  unsigned long xx = *(unsigned long*)x, yy = *(unsigned long*)y;
  if (xx < yy) return -1;
  if (xx > yy) return  1;
  return 0;
}

double Mmin(double x, double y)
{
  if(x < y) return x;
  else return y;
}

double Mmax(double x, double y)
{
  if(x > y) return x;
  else return y;
}
int main(int nargs, char **args)
{
  
  int n, i, p, seed, num_cpus, tst_num, j, k, l , m;
  unsigned long time1, time2;
  double d;
  double best_thread = 0.0;
  double worst_thread = 0.0;
  double avg_thread = 0.0;
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
  volatile double *hitarray;
  
  struct threadstruct *thrdstruct;
  
  
  //retrieve flags specified by person running program
  GetFlags(nargs, args, &clockspeed, &p, &num_cpus, &tst_num);
  //initializes pthread attr's for locking down threads to cores and 
  //init's the mutex lock
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  pthread_mutex_init(&wqlock, NULL);
  
  mythrs = malloc(sizeof(pthread_t)*(p));
  hitarray = malloc(sizeof(long)*(p));
  thrdstruct = malloc(sizeof(struct threadstruct)*(p));
  mywork = malloc(sizeof(struct workq)*(p));
  for(i=0;i<p;i++)
  {
    hitarray[i] = 0.0;
  }

  for(k=0;k<p;k++)
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
    //move to the next corej
    cpu_ctr++;
  }

  //this creates p-1 threads for the program, we dont need 
  //p number threads created since the one we started the program 
  //with is being used for work aswell
  for(i=0;i<tst_num;i++)
  {
    rdtsc(time1);
    
    for(j=0;j<p;j++)
    {
      //tells the thread this is a monte carlo work order
      (mywork+j)->workOrder = 1;
      (mywork+j)->virtualrank = j;
      //add into work queue
      AddWork((mywork+j));
    }

    //i am electing not to use the master thread as a worker thread
    //as i am testing for individual thread timings and a thread that works
    //differently from the rest of the threadpool will pollute my results
    //hitarray[0] = rdtsc(time2);
    //master thread will pause here until all worker threads 
    //have completed work
    j = 0;
    while(1)
    {
      if(j<p)
      {
	if(hitarray[j] != 0)
	{
	  j++;
	}
      }
      else
	break;
    }

    qsort(&hitarray, p, sizeof(long), sort);
    
    fprintf(stderr,"%d of %s\n", __LINE__,__FILE__);

    d = (hitarray[0] - time1) / (clockspeed * 1000000);
    fprintf(stderr,"%d of %s\n", __LINE__,__FILE__);

    best_thread = Mmin(d,best_thread);
    fprintf(stderr,"%d of %s\n", __LINE__,__FILE__);

    d = (hitarray[p] - time1) / (clockspeed * 1000000);
    fprintf(stderr,"%d of %s\n", __LINE__,__FILE__);

    worst_thread = Mmax(d,worst_thread);
    fprintf(stderr,"%d of %s\n", __LINE__,__FILE__);
    while(j>0)
    {
      avg_thread += (hitarray[j] - time1) / (clockspeed * 1000000);
      j--;
    }
  }
  for(l=0;l<p;l++)
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
  fprintf(stdout,"worst thread completion time = %f, best thread completion time = %f, avg thread completion time = %f\n", worst_thread, best_thread, avg_thread);
  //free(mythrs);
}
