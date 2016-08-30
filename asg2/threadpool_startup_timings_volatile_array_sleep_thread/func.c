#include "func.h"

int IsInFunc(double x, double y, double z)
/*
 * RETURNS: 1 if coordinates are in or on the circle of the given radius,
 *          0 otherwise.
 */
{
   double dist;

   dist = sqrt(x*x+y*y+z*z);
   if (dist <= RADIUS)
      return(1);
   return(0);
}

void GetBoundaries(double *xmax, double *xmin, double *ymax, double *ymin,
                   double *zmax, double *zmin)
{
#if 1 
   *xmax = 15.0;
   *xmin = -20.0;
   *ymax = 18.0;
   *ymin = -32.5;
   *zmax = 11.5;
   *zmin = -9.0;
#elif 0 
   *xmax = 5.0;
   *xmin = -5.0;
   *ymax = 6.0;
   *ymin = -4.5;
   *zmax = 8.0;
   *zmin = -4.7;
#else
   *xmax = *ymax = *zmax = 4.5;
   *xmin = *ymin = *zmin = -4.5;
#endif
}
