#include "mytime.h"
#include <stdio.h>

unsigned long long GetCycleCount(void);
double GetWallTime(void)
/*
 * Returns time in seconds
 */
{
   static unsigned long long start=0;
   static const double SPC = 1.0 / (ArchMhz*1000000.0);
   unsigned long long t0;
   if (start)
   {
      t0 = GetCycleCount() - start;
      return(SPC*t0);
   }
   start = GetCycleCount();
   return(0.0);
}
