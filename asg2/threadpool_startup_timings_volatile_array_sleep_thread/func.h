#ifndef FUNC_H
   #define FUNC_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define RADIUS 4.4
int IsInFunc(double x, double y, double z);
void GetBoundaries(double *xmax, double *xmin, double *ymax, double *ymin,
                   double *zmax, double *zmin);

#ifdef DRAND
   #define myrand() drand48()
   #define mysrand(se_) srand48(se_)
#else
   #ifdef _OPENMP
      #define mysrand(se_)
      #define myrand() ((double) (rand_r(&myseed)) / RAND_MAX)
   #else
      #define mysrand(se_) srand(se_)
      #define myrand() ((double) (rand()) / RAND_MAX)
   #endif
#endif

// #define KNOWN_AREA (M_PI*RADIUS*RADIUS)
#define KNOWN_VOL ((4.0/3.0)*M_PI*RADIUS*RADIUS*RADIUS)
#endif
