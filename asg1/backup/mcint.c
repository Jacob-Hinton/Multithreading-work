#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "func.h"
#include "mytime.h"

//add processor affinity
//distrubute remainder better if( rank < N%p) pthread.N++;remainder better if( rank < N%p) pthread.N++;
void PrintUsage(char *name, int iarg, char *arg)
{
    fprintf(stderr, "\nERROR around arg %d (%s).\n", iarg, arg ? arg:"unknown");
    fprintf(stderr, "USAGE: %s [flags], where flags are:\n", name);
    fprintf(stderr, "   -N <#> : set N to #\n");
    fprintf(stderr, "   -p <#> : set p to #\n");
    fprintf(stderr, "   -s <#> : set s to #\n");
    exit(iarg ? iarg : -1);
}

void GetFlags(int nargs, char **args, unsigned int *N, int *seed, int *p)
{
    int i;
    *N = 16000000;
    *seed = 0xFCB321;
    *p = 4;
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

int ParallelMonteCarlo(unsigned int N, int p, int seed)
{
    int n, i;
    int hits = 0;
    pthread_t *mythrs;
    struct hit_struct *hss;

    hss = malloc(sizeof(struct hit_struct)*(p));
    mythrs = malloc(sizeof(pthread_t)*(p-1));
    n = N / p;
    unsigned int n_extra = N % p;
    hits = 0;
    srand48(seed);
    for(i=1; i < p; i++)
    {
        hss[i].N = n;
        if(i <= n_extra)
            hss[i].N++;
        hss[i].seed = lrand48();
        pthread_create(mythrs+i-1, NULL, SerialMonteCarlo, hss+i);
    }
    hss[0].N = n;
    hss[0].seed = lrand48();
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
    GetWallTime();
    int n, i, p, seed;
    unsigned int hits, N;
    double ms, xdist, ydist, zdist, xmax, ymax, zmax, xmin, ymin, zmin, compVol;
    GetBoundaries(&xmax, &xmin, &ymax, &ymin, &zmax, &zmin);

    xdist = xmax - xmin;
    ydist = ymax - ymin;
    zdist = zmax - zmin;

    double rectVol = xdist * ydist * zdist;
    
    GetFlags(nargs, args, &N, &seed, &p);
    hits = ParallelMonteCarlo(N, p, seed);
 
    ms = GetWallTime();
    compVol = rectVol * hits / N;
    fprintf(stdout, "rectVol = %f * %f * %f = %f\n", xdist, ydist, zdist, rectVol);
    fprintf(stdout, "N=%u, nhits=%u, %% hits = %f\n", N, hits, (hits*100.0)/(1.0*N));
    fprintf(stdout, "Time for %d iterations: %f (ms)\n", N, ms);
    fprintf(stdout, "funcVol = %lf, givenVol=%lf error=%e\n",compVol, KNOWN_VOL, fabs(compVol-KNOWN_VOL));
}
