RCWdir = /home/jake/Documents/whaleyWorkSummer/asg1/montecarloasg 
CC = gcc
INCdir = -I
DEFS = -Dx86Mhz=2400
CCFLAGS = $(DEFS) $(INCdir) -O3 -fopenmp -m32
GCC = gcc
GCCFLAGS = $(DEFS) $(INCdir) -O3 -m32
ASG = gcc
ASGFLAGS = $(DEFS) -x assembler-with-cpp -m32

parallel:  xmcint
all: xmcint_s xmcint
serial : xmcint_s
serRCW : xmcint_sRCW
xmcint : mcint.o func.o mytime.o GetCycleCount.o
	$(CC) $(CCFLAGS) -o $@ mcint.o mytime.o GetCycleCount.o func.o -lm
mcint.o : func.h mytime.h mcint.c
	$(CC) $(CCFLAGS) -c -o $@ mcint.c
mytime.o : mytime.h mytime.c
	$(CC) $(CCFLAGS) -c -o $@ mytime.c
func.o : func.c func.h
	$(CC) $(CCFLAGS) -c -o $@ func.c
GetCycleCount.o : GetCycleCount.S
	$(ASG) $(ASGFLAGS) -c -o $@ GetCycleCount.S

xtime : mytime_s.o GetCycleCount.o
	$(GCC) $(GCCFLAGS) -o $@ timetest.c mytime_s.o GetCycleCount.o
xmcint_s : mcint_s.o mytime_s.o func_s.o GetCycleCount.o
	$(GCC) $(GCCFLAGS) -o $@ mcint_s.o mytime_s.o GetCycleCount.o \
	       func_s.o -lm
mcint_s.o : mytime.h func.h mcint.c
	$(GCC) $(GCCFLAGS) -c -o $@ mcint.c
mytime_s.o : mytime.h mytime.c
	$(GCC) $(GCCFLAGS) -c -o $@ mytime.c
func_s.o : func.c func.h
	$(GCC) $(GCCFLAGS) -c -o $@ func.c
xmcint_sRCW : mcint_s.c mytime_s.o func_s.o GetCycleCount.o
	$(GCC) $(GCCFLAGS) -o $@ mcint_s.c mytime_s.o GetCycleCount.o \
	       func_s.o -lm

clean :
	rm -f mcint.o func.o mytime.o mcint_s.o func_s.o mytime_s.o \
	      GetCycleCount.o
kill:
	$(MAKE) clean
	rm -f xtime xmcint_s xmcint xmcint_sRCW
