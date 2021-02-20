/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                         Coleco.h                        **/
/**                                                         **/
/** This file contains declarations relevant to the drivers **/
/** and Coleco emulation itself. See Z80.h for #defines     **/
/** related to Z80 emulation.                               **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#ifndef COLECO_H
#define COLECO_H

#include "SDL.h"
#include "Z80.h"            /* Z80 CPU emulation             */
#include "SN76489.h"        /* SN76489 PSG emulation         */

#include <time.h>

#define NORAM     0xFF      /* Byte to be returned from      */
                            /* non-existing pages and ports  */
#define MAXSCREEN 3         /* Highest screen mode supported */

extern byte AutoA,AutoB;    /* 1: Autofire ON on buttons A,B */
extern byte LogSnd;         /* 1: Log soundtrack into a file */
extern byte Adam;           /* 1: Emulate Coleco Adam        */
extern int  VPeriod;        /* CPU cycles/VBlank             */
extern int  HPeriod;        /* CPU cycles/HBlank             */
extern byte UPeriod;   /* Number of interrupts/screen update */
/*************************************************************/

/***** Following are macros to be used in screen drivers *****/
#define BigSprites    (VDP[1]&0x01)   /* Zoomed sprites      */
#define Sprites16x16  (VDP[1]&0x02)   /* 16x16/8x8 sprites   */
#define ScreenON      (VDP[1]&0x40)   /* Show screen         */
/*************************************************************/

extern Z80 CPU;                       /* CPU registers+state */
extern byte *VRAM;
extern byte *RAM;               /* Main and Video RAMs */

extern char *SndName;                 /* Soundtrack log file */
extern char *PrnName;                 /* Printer redir. file */

extern byte *ChrGen,*ChrTab,*ColTab;  /* VDP tables [screens]*/
extern byte *SprGen,*SprTab;          /* VDP tables [sprites]*/
extern byte FGColor,BGColor;          /* Colors              */
extern byte ScrMode;                  /* Current screen mode */
extern byte VDP[8],VDPStatus;         /* VDP registers       */

extern word JoyState[2];              /* Joystick states     */
extern byte ExitNow;                  /* 1: Exit emulator    */

//LUDO:
# define CV_RENDER_NORMAL     0
# define CV_RENDER_FIT        1
# define CV_LAST_RENDER       1

# define MAX_PATH           256
# define CV_MAX_SAVE_STATE 5
# define CV_MAX_CHEAT      10

#define CV_WIDTH  272
#define CV_HEIGHT 208

#define SNAP_WIDTH   136
#define SNAP_HEIGHT  104

#define CV_CHEAT_NONE    0
#define CV_CHEAT_ENABLE  1
#define CV_CHEAT_DISABLE 2

#define CV_CHEAT_COMMENT_SIZE 25
#define CV_CHEAT_RAM_SIZE 0x8000 

  
  typedef struct CV_cheat_t {
    unsigned char  type;
    unsigned short addr;
    unsigned char  value;
    char           comment[CV_CHEAT_COMMENT_SIZE];
  } CV_cheat_t;


  typedef struct colem_save_t {

  Z80 CPU;               /* Z80 CPU registers and state   */
  SN76489 PSG;           /* SN76489 PSG state             */

  byte JoyMode;          /* Joystick controller mode      */
  word JoyState[2];      /* Joystick states               */
  
  long  ChrGen_idx;
  long  ChrTab_idx;
  long  ColTab_idx;
  long  SprGen_idx;
  long  SprTab_idx;                  /* VDP tables (sprites)          */

  pair WVAddr;
  pair RVAddr;                   /* Storage for VRAM addresses    */
  byte VKey;                     /* VDP address latch key         */
  byte FGColor;
  byte BGColor;          /* Colors                        */
  byte ScrMode;                  /* Current screen mode           */
  byte CurLine;                  /* Current scanline              */
  byte VDP[8];
  byte VDPStatus;         /* VDP registers                 */

} colem_save_t;

  typedef struct CV_save_t {

    SDL_Surface    *surface;
    char            used;
    char            thumb;
    time_t          date;

  } CV_save_t;
  
  typedef struct ColecoVision_t {
 
    CV_save_t cv_save_state[CV_MAX_SAVE_STATE];
    CV_cheat_t cv_cheat[CV_MAX_CHEAT];

    int  comment_present;
    char cv_save_name[MAX_PATH];
    char cv_home_dir[MAX_PATH];
    int  psp_screenshot_id;
    int  psp_cpu_clock;
    int  psp_reverse_analog;
    int  cv_view_fps;
    int  cv_current_clock;
    int  cv_vsync;
    int  cv_ntsc;
    int  cv_auto_fire;
    int  cv_auto_fire_period;
    int  cv_auto_fire_pressed;
    int  cv_current_fps;
    int  psp_active_joystick;
    int  cv_snd_enable;
    int  cv_render_mode;
    int  cv_speed_limiter;
    int  psp_skip_max_frame;
    int  psp_skip_cur_frame;

  } ColecoVision_t;

  extern ColecoVision_t CV;

/** StartColeco() ********************************************/
/** Allocate memory, load ROM image, initialize hardware,   **/
/** CPU and start the emulation. This function returns 0 in **/
/** the case of failure.                                    **/
/*************************************************************/
extern int StartColeco();

/** TrashColeco() ********************************************/
/** Free memory allocated by StartColeco().                 **/
/*************************************************************/
void TrashColeco(void);

/** InitMachine() ********************************************/
/** Allocate resources needed by the machine-dependent code.**/
/************************************ TO BE WRITTEN BY USER **/
int InitMachine(void);

/** TrashMachine() *******************************************/
/** Deallocate all resources taken by InitMachine().        **/
/************************************ TO BE WRITTEN BY USER **/
void TrashMachine(void);

/** RefreshLine#() *******************************************/
/** Refresh line Y (0..191), on an appropriate SCREEN#,     **/
/** including sprites in this line.                         **/
/************************************ TO BE WRITTEN BY USER **/
void RefreshLine0(byte Y);
void RefreshLine1(byte Y);
void RefreshLine2(byte Y);
void RefreshLine3(byte Y);

/** RefreshScreen() ******************************************/
/** Refresh screen. This function is called in the end of   **/
/** refresh cycle to show the entire screen.                **/
/************************************ TO BE WRITTEN BY USER **/
void RefreshScreen(void);

/** SetColor() ***********************************************/
/** Set color N (0..15) to (R,G,B).                         **/
/************************************ TO BE WRITTEN BY USER **/
void SetColor(byte N,byte R,byte G,byte B);

/** Joysticks() **********************************************/
/** This function is called to poll joysticks. It should    **/
/** set JoyState[0]/JoyState[1] in a following way:         **/
/**                                                         **/
/**      x.FIRE-B.x.x.L.D.R.U.x.FIRE-A.x.x.N3.N2.N1.N0      **/
/**                                                         **/
/** Least significant bit on the right. Active value is 0.  **/
/************************************ TO BE WRITTEN BY USER **/
void Joysticks(void);

/** Sound() **************************************************/
/** Set sound volume (0..255) and frequency (Hz) for a      **/
/** given channel (0..3). This function is only needed with **/
/** #define SOUND. The 3rd channel is noise.                **/
/************************************ TO BE WRITTEN BY USER **/
void Sound(int Channel,int Freq,int Volume);

void cv_set_video_mode( int ntsc );

#endif /* COLECO_H */
