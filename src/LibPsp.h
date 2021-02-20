/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        LibUnix.h                        **/
/**                                                         **/
/** This file contains definitions and declarations for     **/
/** some commonly used Unix/X11 routines.                   **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#ifndef LIBPSP_H
#define LIBPSP_H
#ifdef UNIX

#ifdef BPP32
typedef unsigned int pixel;
#else
#ifdef BPP16
typedef unsigned short pixel;
#else
#ifdef BPP8
typedef unsigned char pixel;
#endif
#endif
#endif

/** InitLibUnix() ********************************************/
/** This function is called to obtain the display and other **/
/** values at startup. It returns 0 on failure.             **/
/*************************************************************/

/*** Color Allocation ****************************************/
#define MAXALLOC  4096

/** Timers ***************************************************/
#define MAXTIMERFREQ 100 /* Maximal frequency for AddTimer() */

int TimerSignal(int Freq,void (*Handler)(int));
    /* Establishes a signal handler called with given        */
    /* frequency (Hz). Returns 0 if failed.                  */
int AddTimer(int Freq,void (*Handler)(void));
    /* Establishes a periodically called routine at a given  */
    /* frequency (1..MAXTIMERFREQ Hz). Returns 0 if failed.  */
void DelTimer(void (*Handler)(void));
    /* Removes timers established with AddTimer().           */
/*************************************************************/

#endif /* UNIX */
#endif /* LIBUNIX_H */
