#define _GNU_SOURCE 1
#define __USE_GNU 1
#include <sched.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "func.h"
#include "mytime.h"

//add processor affinity
void PrintUsage(char *name, int iarg, char *arg)
{
    fprintf(stderr, "\nERROR around arg %d (%s).\n", iarg, arg ? arg:"unknown");
    fprintf(stderr, "USAGE: %s [flags], where flags are:\n", name);
    fprintf(stderr, "   -N <#> : set N to #\n");
    fprintf(stderr, "   -p <#> : set p to #\n");
    fprintf(stderr, "   -s <#> : set s to #\n");
    fprintf(stderr, "   -c <#> : set num_cores to #\n");
    fprintf(stderr, "   -t <#> : set tst_num to #\n");
    exit(iarg ? iarg : -1);
}

void GetFlags(int nargs, char **args, unsigned int *N, int *seed, int *p, int *num_cpus, int *tst_num)
{
    int i;
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

struct hit_struct
{
    int N;
    unsigned int seed;
    unsigned int hitout;
};

void *SerialMonteCarlo(void *vds)
{
    double x, y, z;
    int i, hits;
    double xmax, xmin, xdist, ymax, ymin, ydist, zmax, zmin, zdist;
    struct hit_struct *hs = vds;
    hits = 0;
    GetBoundaries(&xmax, &xmin, &ymax, &ymin, &zmax, &zmin);

    xdist = xmax - xmin;
    ydist = ymax - ymin;
    zdist = zmax - zmin;
    for (i=0; i < hs->N; i++)
    {
        x = ((double) (rand_r(&hs->seed)) / RAND_MAX) *xdist + xmin;
        y = ((double) (rand_r(&hs->seed)) / RAND_MAX) *ydist + ymin;
        z = ((double) (rand_r(&hs->seed)) / RAND_MAX) *zdist + zmin;
        if (IsInFunc(x, y, z)) 
            hits++;
    }
    hs->hitout = hits;
    return(NULL);
}

int ParallelMonteCarlo(unsigned int N, int p, long int seed, int num_cpus)
{
    int n, i;
    int hits = 0;
    int cpu_ctr = 1;
    pthread_attr_t attr;
    cpu_set_t cpuset;
    pthread_t *mythrs;
    struct hit_struct *hss;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    hss = malloc(sizeof(struct hit_struct)*(p));
    mythrs = malloc(sizeof(pthread_t)*(p-1));
    n = N / p;
    unsigned int n_extra = N % p;
    hits = 0;
    for(i=1; i < p; i++)
    {
        hss[i].N = n;
        if(i <= n_extra)
            hss[i].N++;
        hss[i].seed = seed;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_ctr, &cpuset);
        pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);
        pthread_create(mythrs+i-1, &attr, SerialMonteCarlo, hss+i);
        if (num_cpus == cpu_ctr)
            cpu_ctr = 0;
        else
            cpu_ctr++;
    }
    hss[0].N = n;
    hss[0].seed = seed;
    SerialMonteCarlo(hss);
    hits = hss[0].hitout;
    for (i = 1; i < p; i++)
    {
        pthread_join(mythrs[i-1], NULL);
        hits += hss[i].hitout;
    }

    free(hss);
    free(mythrs);
    return hits;
}

int main(int nargs, char **args)
{
    int n, i, p, seed, num_cpus, tst_num;
    unsigned int hits, N;
    double calcms, xdist, ydist, zdist, xmax, ymax, zmax, xmin, ymin, zmin, compVol;
    double ms, best_time, worst_time, avg_time = 0;
    GetBoundaries(&xmax, &xmin, &ymax, &ymin, &zmax, &zmin);

    xdist = xmax - xmin;
    ydist = ymax - ymin;
    zdist = zmax - zmin;

    double rectVol = xdist * ydist * zdist;
    
    GetFlags(nargs, args, &N, &seed, &p, &num_cpus, &tst_num);
    srand48(seed);
    for(i=0;i<tst_num;i++)
    {
        calcms = GetWallTime();
        hits = ParallelMonteCarlo(N, p, lrand48(), num_cpus-1);
        ms = GetWallTime() - calcms;
        avg_time += ms;
        compVol = rectVol * hits / N;
        if(i == 0)
        {
            best_time = ms;
            worst_time = ms;
        }
        else
        {
            if(best_time > ms)
                best_time = ms;
            if(worst_time < ms)
                worst_time = ms;
        }

        fprintf(stdout, "rectVol = %f * %f * %f = %f\n", xdist, ydist, zdist, rectVol);
        fprintf(stdout, "N=%u, nhits=%u, %% hits = %f\n", N, hits, (hits*100.0)/(1.0*N));
        fprintf(stdout, "Time for %d iterations: %f (ms)\n", N, ms);
        fprintf(stdout, "funcVol = %lf, givenVol=%lf error=%e\n",compVol, KNOWN_VOL, fabs(compVol-KNOWN_VOL));
    }
        fprintf(stdout, "number of threads = %d, number of cores = %d\n", p, num_cpus);
        fprintf(stdout, "avg time = %f best time = %f worst time = %f\n", GetWallTime() / tst_num, best_time, worst_time);
}
