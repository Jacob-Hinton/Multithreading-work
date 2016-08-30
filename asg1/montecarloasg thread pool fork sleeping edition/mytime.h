#ifndef MYTIME_H
   #define MYTIME_H

#ifndef ArchMhz
   #ifdef SparcMhz
      #define ArchMhz SparcMhz
   #elif defined(x86Mhz)
      #define ArchMhz x86Mhz
   #else
      #error "Must define x86Mhz or SparcMhz!"
   #endif
#endif
unsigned long long GetCycleCount(void);
double GetWallTime(void);

#endif
