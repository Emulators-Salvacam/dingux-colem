/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        LibUnix.c                        **/
/**                                                         **/
/** This file contains implementation for some commonly     **/
/** used Unix/X11 routines.                                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#include "LibPsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#define NEXT(N) ((N+1)%MAXALLOC)  /* Used when allocating colors */

struct Timer
{
  struct Timer *Next;
  int Freq;
  void (*Handler)(void);
};

static unsigned long Black;
static unsigned long ColorPen[MAXALLOC];
static int ColorQueue[MAXALLOC];
static int ColorValue[MAXALLOC];
static int ColorCount[MAXALLOC];
static int AllocLo   = 0;
static int AllocHi   = 1;

struct Timer *FirstTimer = NULL;
static int TimerLock     = 0;

static void MasterTimer(int Arg);

/** InitLibUnix() ********************************************/
/** This function is called to obtain the display and other **/
/** values at startup. It returns 0 on failure.             **/
/*************************************************************/
int InitLibUnix()
{
  int J;

  /* X11 display */
# if 0 //LUDO:
  Dsp=AppDisplay;
  Scr=DefaultScreenOfDisplay(Dsp);
  CMap=DefaultColormapOfScreen(Scr);
  Black=BlackPixelOfScreen(Scr);
# endif

  /* Color allocation */
  for(J=0;J<MAXALLOC;J++) ColorCount[J]=0;
  AllocLo=0;AllocHi=1;

  /* Timers */  
  FirstTimer=NULL;
  TimerLock=0;

  return(1);
}

/** MasterTimer() ********************************************/
/** The main timer handler which is called MAXTIMERFREQ     **/
/** times a second. It then calls user-defined timers.      **/
/*************************************************************/
static void MasterTimer(int Arg)
{
  static unsigned int Counter=0;
  register struct Timer *P;

  if(!TimerLock)
    for(P=FirstTimer;P;P=P->Next)
      if(!(Counter%P->Freq)) (P->Handler)();

  Counter++;
  signal(Arg,MasterTimer);
}

/** TimerSignal() ********************************************/
/** Establish a signal handler called with given frequency  **/
/** (Hz). Returns 0 if failed.                              **/
/*************************************************************/
int TimerSignal(int Freq,void (*Handler)(int))
{
# if 0 //LUDO: 
  struct itimerval TimerValue;

  TimerValue.it_interval.tv_sec  =
  TimerValue.it_value.tv_sec     = 0;
  TimerValue.it_interval.tv_usec =
  TimerValue.it_value.tv_usec    = 1000000L/Freq;
  if(setitimer(ITIMER_REAL,&TimerValue,NULL)) return(0);
  signal(SIGALRM,Handler);
  return(1);
# endif
}

/** AddTimer() ***********************************************/
/** Establish a periodically called routine at a given      **/
/** frequency (1..MAXTIMERFREQ Hz). Returns 0 if failed.    **/
/*************************************************************/
int AddTimer(int Freq,void (*Handler)(void))
{
  struct Timer *P;

  /* Freq must be 1..MAXTIMERFREQ */
  if((Freq<1)||(Freq>MAXTIMERFREQ)) return(0);

  /* Lock the timer queue */
  TimerLock=1;

  /* Look if this timer is already installed */
  for(P=FirstTimer;P;P=P->Next)
    if(P->Handler==Handler)
    { P->Freq=Freq;TimerLock=0;return(1); }

  /* Make up a new one if not */
  if(FirstTimer)
  {
    for(P=FirstTimer;P->Next;P=P->Next);
    P->Next=malloc(sizeof(struct Timer));
    P=P->Next;
  }
  else
  {
    /* Allocate the first timer */
    FirstTimer=malloc(sizeof(struct Timer));
    P=FirstTimer;

    /* Start up the main handler */
    if(P)
      if(!TimerSignal(MAXTIMERFREQ,MasterTimer))
      { free(P);P=NULL; }
  }

  /* Set up the timer and exit */
  if(P) { P->Next=0;P->Handler=Handler;P->Freq=Freq; }
  TimerLock=0;
  return(P!=0);
}

/** DelTimer() ***********************************************/
/** Remove routine added with AddTimer().                   **/
/*************************************************************/
void DelTimer(void (*Handler)(void))
{
# if 0 //LUDO: 
  struct itimerval TimerValue;
  struct Timer *P,*T;

  TimerLock=1;

  if(FirstTimer)
    if(FirstTimer->Handler==Handler)
    {
      /* Delete first timer and free the space */
      P=FirstTimer;FirstTimer=P->Next;free(P);

      /* If no timers left, stop the main handler */
      if(!FirstTimer)
      {
         TimerValue.it_interval.tv_sec  =
         TimerValue.it_value.tv_sec     =
         TimerValue.it_interval.tv_usec =
         TimerValue.it_value.tv_usec    = 0;
         setitimer(ITIMER_REAL,&TimerValue,NULL);
         signal(SIGALRM,SIG_DFL);
      }
    }
  else
  {
    /* Look for our timer */
    for(P=FirstTimer;P;P=P->Next)
      if(P->Next->Handler==Handler) break;

    /* Delete the timer and free the space */
    if(P) { T=P->Next;P->Next=T->Next;free(T); }
  }

  TimerLock=0;
# endif
}

