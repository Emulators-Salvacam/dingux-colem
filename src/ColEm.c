/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                          ColEm.c                        **/
/**                                                         **/
/** This file contains generic main() procedure statrting   **/
/** the emulation.                                          **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "Coleco.h"

/** main() ***************************************************/
/** This is a main() function used in Unix and MSDOS ports. **/
/** It parses command line arguments, sets emulation        **/
/** parameters, and passes control to the emulation itself. **/
/*************************************************************/
int 
SDL_main(int argc,char *argv[])
{
  memset(&CV, 0, sizeof(ColecoVision_t));
  strcpy(CV.cv_home_dir,".");

  psp_sdl_init();

  if(!InitMachine()) {
    return(1);
  }

  StartColeco();
  TrashColeco();
  TrashMachine();


  return(0);
}
