#define _GNU_SOURCE 1
#define __USE_GNU 1
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "func.h"
#include "mytime.h"

//this first global int are where the individual threads 
//add their results from testing the second is iterated everytime 
//a thread completes their work so the master knows when
//to continue with adding new work or closing threads

int totalhits = 0;
int hitcounter = 0;
static pthread_mutex_t wqlock;

//this program utilizes a work queue to deliver work orders 
//to the threads which constantly poll for more work

struct workq
{
    struct workq *next;

    //values needed for montecarlo integration
    int N;
    unsigned int seed;

    //this is a meta value that tells the thread what type of work will be done.
    //for now, 1 means perform montecarlo integration while 
    //0 means kill the thread.
    int workOrder;
} *workhead;

//function shows user what flags do what in the program

void PrintUsage(char *name, int iarg, char *arg)
{
    fprintf(stderr, "\nERROR around arg %d (%s).\n", iarg, arg ? arg:"unknown");
    fprintf(stderr, "USAGE: %s [flags], where flags are:\n", name);
    fprintf(stderr, "   -N <#> : set Number of iterations to #\n");
    fprintf(stderr, "   -p <#> : set number of threads to #\n");
    fprintf(stderr, "   -s <#> : set seed to #\n");
    fprintf(stderr, "   -c <#> : set number of cores to #\n");
    fprintf(stderr, "   -t <#> : set number of tests to #\n");
    fprintf(stderr,"program should be run with taskset to lock down\ 
            master thread to core 0\n");
    exit(iarg ? iarg : -1);
}

//this function retrieves all flags needed for the program

void GetFlags(int nargs, char **args, unsigned int *N,
        int *seed, int *p, int *num_cpus, int *tst_num)
{
    int i;

    //the following values are the default flags and will be used if 
    //a flag is not explicitly given

    *N = 16000000;
    *seed = 0xFCB321;
    *p = 4;
    *num_cpus = 4;
    *tst_num = 50;

    for (i=1; i < nargs; i++)
    {
        if(args[i][0] != '-')
            PrintUsage(args[0], i, args[i]);
        switch(args[i][1])
        {
        case 'N':
            if (++i >= nargs)
                PrintUsage(args[0], i, "out of arguments");
            *N = atoi(args[i]);
            break;
        case 's':
            if (++i >= nargs)
                PrintUsage(args[0], i, "out of arguments");
            *seed = atoi(args[i]);
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

void *SerialMonteCarlo()
{
    double x, y, z;
    int i, hits;
    double xmax, xmin, xdist, ymax, ymin, ydist, zmax, zmin, zdist;
    struct workq *mywork;

    //this function in func.c returns the needed params for the 
    //object encasing the object whose integral we are trying to find

    GetBoundaries(&xmax, &xmin, &ymax, &ymin, &zmax, &zmin);

    xdist = xmax - xmin;
    ydist = ymax - ymin;
    zdist = zmax - zmin;
    
    //here we create an infinite loop for the thread so 
    //that it can constantly poll for the next work order

    while(1)
    {
        if(workhead)
        {
            //we need a lock here to prevent two threads 
            //competing for the same work order
            pthread_mutex_lock(&wqlock);
            //we need to check again after passing the lock
            //for the work head in case the previous
            //thread took the last work order
            if(workhead)
            {
                //if there is still a workorder left, 
                //remove it from the queue and unlock the threads
                mywork = workhead;
                workhead = workhead->next;
                pthread_mutex_unlock(&wqlock);

                //workorder 0 has the thread exit after 
                //incrementing hitcounter to notify the master 
                //it has completed work
                if(mywork->workOrder == 0)
                {
                    //lock around inrementation of hitcounter to prevent
                    //race condition
                    pthread_mutex_lock(&wqlock);
                    hitcounter++;
                    pthread_mutex_unlock(&wqlock);
                    pthread_exit(NULL);
                }
                hits = 0;
                
                //this loop will generate random values for x, y, and z params
                //that are inside the params given to us by
                //GetBoundaries and check if they are inside the params of 
                //the object who's integral we are looking for using IsInFunc
                for (i=0; i < mywork->N; i++)
                {
                    x = ((double) (rand_r(&mywork->seed)) / RAND_MAX) 
                        *xdist + xmin;
                    y = ((double) (rand_r(&mywork->seed)) / RAND_MAX) 
                        *ydist + ymin;
                    z = ((double) (rand_r(&mywork->seed)) / RAND_MAX) 
                        *zdist + zmin;
                    if (IsInFunc(x, y, z))
                    hits++;
                }
                //we need to locks around these variables since they
                //are global and we want to prevent race conditions
                pthread_mutex_lock(&wqlock);
                totalhits += hits;
                hitcounter++;
                pthread_mutex_unlock(&wqlock);
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

//this is an serial implementation of montecarlo integration that is
//used by the master thread.

void LocalMonteCarlo(int N, unsigned int seed)
{
    double x, y, z;
    int i;
    double xmax, xmin, xdist, ymax, ymin, ydist, zmax, zmin, zdist;
    int hitnum = 0;
    GetBoundaries(&xmax, &xmin, &ymax, &ymin, &zmax, &zmin);
    xdist = xmax - xmin;
    ydist = ymax - ymin;
    zdist = zmax - zmin;
    for (i=0; i < N; i++)
    {
         x = ((double) (rand_r(&seed)) / RAND_MAX) *xdist + xmin;
         y = ((double) (rand_r(&seed)) / RAND_MAX) *ydist + ymin;
         z = ((double) (rand_r(&seed)) / RAND_MAX) *zdist + zmin;
         if (IsInFunc(x, y, z))
             hitnum++;
    }
    //still need to lock here since these are global ints and the 
    //other threads can still be trying to edit at the same time
    pthread_mutex_lock(&wqlock);
    totalhits += hitnum;
    hitcounter++;
    pthread_mutex_unlock(&wqlock);
    return;
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
    int n, i, p, seed, num_cpus, tst_num, j;
    //we start cpu_ctr at 1 so that threads start spawning on the second 
    //core since the master thread is already tied down from taskset
    int cpu_ctr = 1; 
    unsigned int N, n_extra;
    struct workq mywork;
    double calcms, xdist, ydist, zdist, xmax, ymax, zmax, xmin, ymin, zmin, compVol;
    double low_err, high_err, curr_err, avg_err = 0;
    double ms, best_time, worst_time, avg_time = 0;
    pthread_attr_t attr;
    cpu_set_t cpuset;
    pthread_t *mythrs;

    //retrieve params for object surrounding the object we are 
    //finding the integral of
    GetBoundaries(&xmax, &xmin, &ymax, &ymin, &zmax, &zmin);

    xdist = xmax - xmin;
    ydist = ymax - ymin;
    zdist = zmax - zmin;

    double rectVol = xdist * ydist * zdist;
    
    //retrieve flags specified by person running program
    GetFlags(nargs, args, &N, &seed, &p, &num_cpus, &tst_num);

    //initializes pthread attr's for locking down threads to 
    //cores and init's the mutex lock
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_mutex_init(&wqlock, NULL);

    mythrs = malloc(sizeof(pthread_t)*(p));

    //in the event that the number of iterations does not divide evenly
    //over all threads the remainder of the division is placed in 
    //n_extra that will distribute these extra iterations to the threads
    n = N / p;
    n_extra = N % p;

    //inits the seed
    srand48(seed);
    
    //this creates p-1 threads for the program, we dont need p number
    //threads created since the one we started the program with is 
    //being used for work aswell
    for(i=1;i<p;i++)
    {
        //this checks if the core we are trying to create a thread on exists
        //if it does not, we roll back the core counter to the first core
        if(cpu_ctr == num_cpus)
            cpu_ctr = 0;
        //this clears our cpu set
        CPU_ZERO(&cpuset);
        //this adds the core we want to add a pthread to into the cpu set
        CPU_SET(cpu_ctr, &cpuset);

        //this makes the core we wish to make a pthread on one of 
        //our pthread's attr's 
        pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);
        //by making our pthread asynchronous it can be exited 
        //at any time in its execution
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        pthread_create(mythrs+i-1, &attr, SerialMonteCarlo, NULL);
        //move to the next core
        cpu_ctr++;
    }
    //we now run all through all of our testing
    for(i=0;i<tst_num;i++)
    {
        totalhits = 0;
        calcms = GetWallTime();
        //adds necessary values for testing to the work order
        for(j=0;j<p-1;j++)
        {
            mywork.N = n;
            if(j<n_extra)
                mywork.N++;
            mywork.seed = lrand48();
            //tells the thread this is a monte carlo work order
            mywork.workOrder = 1;
            //add into work queue
            AddWork(&mywork);
        }

        //performs monte carlo on master thread
        //we dont need to worry about adding n_extra here since the remainder
        //cant be spread evenly across all threads
        LocalMonteCarlo(n,lrand48());
        
        //master thread will pause here until all worker threads 
        //have completed work
        while(hitcounter < p);
        //reset work counter
        hitcounter = 0;
        //this places calculation time for last work order in ms
        ms = GetWallTime() - calcms;

        //the below calculates the best, worst, and average case for 
        //error and time of the work order
        avg_time += ms;
        compVol = rectVol * totalhits / N;
        curr_err = fabs(compVol-KNOWN_VOL);
        avg_err += curr_err;

        if(i == 0)
        {
            best_time = ms;
            worst_time = ms;
            high_err = curr_err;
            low_err = curr_err;
        }

        else
        {
            if(best_time > ms)
                best_time = ms;
            if(worst_time < ms)
                worst_time = ms;
            if(high_err < curr_err)
                high_err = curr_err;
            if(low_err > curr_err)
                low_err = curr_err;
        }

        fprintf(stdout, "rectVol = %f * %f * %f = %f\n",
                xdist, ydist, zdist, rectVol);
        fprintf(stdout, "N=%u, nhits=%u, %% hits = %f\n",
                N, totalhits, (totalhits*100.0)/(1.0*N));
        fprintf(stdout, "Time for %d iterations: %f (ms)\n", N, ms);
        fprintf(stdout, "funcVol = %lf, givenVol=%lf error=%e\n"
                ,compVol, KNOWN_VOL, fabs(compVol-KNOWN_VOL));
    }

    //final testing results
    fprintf(stdout, "\nnumber of threads = %d, number of cores = %d\n"
            , p, num_cpus);
    fprintf(stdout, "avg time = %f best time = %f worst time = %f\n"
            , GetWallTime() / tst_num, best_time, worst_time);
    fprintf(stdout, "avg err = %e lowest err = %e highest err = %e\n"
            , avg_err / tst_num, low_err, high_err);
    
    //we add p-1 work orders with the exit command to kill off the threads 
    //we've spawned
    for(i=0;i<p-1;i++)
    {
       mywork.workOrder = 0;
       AddWork(&mywork);
    }
    //this loop checks for all threads to be completed so that we dont try 
    //freeing memory that is in use at the end
    for(i=0;i<p-1;i++)
    {
        pthread_join(*(mythrs+i), NULL);
    }

    free(mythrs);
}
