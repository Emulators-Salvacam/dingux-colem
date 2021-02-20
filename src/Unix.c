/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                           Unix.c                        **/
/**                                                         **/
/** This file contains Unix/X-dependent subroutines and     **/
/** drivers. It includes common drivers from Common.h.      **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

/** Private #includes ****************************************/
#include "Coleco.h"
#include "LibPsp.h"

#include "psp_kbd.h"

/** Standard Unix/X #includes ********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "global.h"
#include "psp_sdl.h"
#include "psp_sound.h"

/** Public parameters ****************************************/
int SaveCPU  = 1;               /* 1 = freeze when focus out */
int UseSHM   = 1;               /* 1 = use MITSHM            */
int UseSound = 0;               /* 1 = use sound             */

//LUDO:
int psp_screenshot_mode = 0;


static byte *XBuf;
static byte XPal[16];
static byte XPal0;

/** Sound-related definitions ********************************/
static int SndSwitch = 0x0F;
static int SndVolume = 192;

/** This function is called on signals ***********************/
static void OnBreak(int Arg) { ExitNow=1;signal(Arg,OnBreak); }

/** InitMachine() ********************************************/
/** Allocate resources needed by Unix/X-dependent code.     **/
/*************************************************************/
int InitMachine(void)
{
  int J;
  for(XPal0=0,J=0;J<16;J++) XPal[J]=J;

  /* Initialize LibUnix toolkit */
  if(!InitLibUnix()) { return(0); }

  XBuf = (byte *)malloc(CV_WIDTH * CV_HEIGHT * sizeof(byte));

  return(1);
}

/** TrashMachine() *******************************************/
/** Deallocate all resources taken by InitMachine().        **/
/*************************************************************/
void 
TrashMachine(void)
{
  audio_shutdown();
}

/** PutImage() ***********************************************/
/** Put an image on the screen.                             **/
/*************************************************************/
  extern SDL_Surface *back_surface;
  extern unsigned short Palette_rgb[16];

void 
PutImage_normal(void)
{ 
  short *scr_ptr = (short *)back_surface->pixels;
  byte *buf_ptr = XBuf;

  scr_ptr += 24 + (PSP_LINE_SIZE * 16);
  
  int y = CV_HEIGHT;
  while (y-- > 0) {
    int x = CV_WIDTH;
    while (x-- > 0) {
      *scr_ptr++ = Palette_rgb[(*buf_ptr++)& 0xf];
    }
    scr_ptr += PSP_LINE_SIZE - CV_WIDTH;
  }
}

void 
PutImage_blit(void)
{ 
  short *scr_ptr = (short *)blit_surface->pixels;
  byte *buf_ptr = XBuf;

  int y = CV_HEIGHT;
  while (y-- > 0) {
    int x = CV_WIDTH;
    while (x-- > 0) {
      *scr_ptr++ = Palette_rgb[(*buf_ptr++)& 0xf];
    }
  }
}

void
PutImage_fit()
{
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = CV_WIDTH;
  srcRect.h = CV_HEIGHT;
  dstRect.x = 0;
  dstRect.y = 0;
  dstRect.w = 320;
  dstRect.h = 240;

  PutImage_blit();
  SDL_SoftStretch( blit_surface, &srcRect, back_surface, &dstRect );
}


/** Joysticks ************************************************/
/** Check for keyboard events, parse them, and modify       **/
/** joystick controller status                              **/
/*************************************************************/
void 
Joysticks(void)
{
  psp_update_keys();
}

/** Part of the code common for Unix/X and MSDOS drivers ********/
static void RefreshSprites(byte Y);
static void RefreshBorder(byte Y);

/** RefreshScreen() ******************************************/
/** Refresh screen. This function is called in the end of   **/
/** refresh cycle to show the entire screen.                **/
/*************************************************************/

void
psp_cv_wait_vsync()
{
# if 0 //ZX: TO_BE_DONE ! 
# ifndef LINUX_MODE
  static int loc_pv = 0;
  int cv = sceDisplayGetVcount();
  if (loc_pv == cv) {
    sceDisplayWaitVblankCB();
  }
  loc_pv = sceDisplayGetVcount();
# endif
# endif
}

void
cv_synchronize(void)
{
  static u32 nextclock = 1;
  static u32 next_sec_clock = 0;
  static u32 cur_num_frame = 0;

  u32 curclock = SDL_GetTicks();

  if (CV.cv_speed_limiter) {
    while (curclock < nextclock) {
     curclock = SDL_GetTicks();
    }
    u32 f_period = 1000 / CV.cv_speed_limiter;
    nextclock += f_period;
    if (nextclock < curclock) nextclock = curclock + f_period;
  }

  if (CV.cv_view_fps) {
    cur_num_frame++;
    if (curclock > next_sec_clock) {
      next_sec_clock = curclock + 1000;
      CV.cv_current_fps = cur_num_frame;
      cur_num_frame = 0;
    }
  }
}

void 
RefreshScreen(void) 
{
  if (CV.psp_skip_cur_frame <= 0) {

    CV.psp_skip_cur_frame = CV.psp_skip_max_frame;

    if (CV.cv_render_mode == CV_RENDER_NORMAL) PutImage_normal(); 
    else                                       PutImage_fit();

    if (psp_kbd_is_danzeff_mode()) {
      danzeff_moveTo(-30, -65);
      danzeff_render();
    }

    if (CV.cv_view_fps) {
      char buffer[32];
      sprintf(buffer, "%03d %3d", CV.cv_current_clock, (int)CV.cv_current_fps );
      psp_sdl_fill_print(0, 0, buffer, 0xffffff, 0 );
    }

    if (CV.cv_vsync) {
      psp_cv_wait_vsync();
    }
    psp_sdl_flip();
  
    if (psp_screenshot_mode) {
      psp_screenshot_mode--;
      if (psp_screenshot_mode <= 0) {
        psp_sdl_save_screenshot();
        psp_screenshot_mode = 0;
      }
    }

  } else if (CV.psp_skip_max_frame) {
    CV.psp_skip_cur_frame--;
  }

  cv_synchronize();
}

/** RefreshBorder() ******************************************/
/** This function is called from RefreshLine#() to refresh  **/
/** the screen border.                                      **/
/*************************************************************/
void RefreshBorder(register byte Y)
{
  if(!Y)
    memset(XBuf,XPal[BGColor],CV_WIDTH*(CV_HEIGHT-192)/2);
  if(Y==191)
    memset(XBuf+CV_WIDTH*(CV_HEIGHT+192)/2,XPal[BGColor],CV_WIDTH*(CV_HEIGHT-192)/2);  
}

/** RefreshSprites() *****************************************/
/** This function is called from RefreshLine#() to refresh  **/
/** sprites.                                                **/
/*************************************************************/
void RefreshSprites(register byte Y)
{
  register byte C,H;
  register byte *P,*PT,*AT;
  register int L,K;
  register unsigned int M;

  H=Sprites16x16? 16:8;
  C=0;M=0;L=0;AT=SprTab-4;
  do
  {
    M<<=1;AT+=4;L++;    /* Iterating through SprTab */
    K=AT[0];            /* K = sprite Y coordinate */
    if(K==208) break;   /* Iteration terminates if Y=208 */
    if(K>256-H) K-=256; /* Y coordinate may be negative */

    /* Mark all valid sprites with 1s, break at 4 sprites */
    if((Y>K)&&(Y<=K+H)) { M|=1;if(++C==4) break; }
  }
  while(L<32);

  for(;M;M>>=1,AT-=4)
    if(M&1)
    {
      C=AT[3];                  /* C = sprite attributes */
      L=C&0x80? AT[1]-32:AT[1]; /* Sprite may be shifted left by 32 */
      C&=0x0F;                  /* C = sprite color */

      if((L<256)&&(L>-H)&&C)
      {
        K=AT[0];                /* K = sprite Y coordinate */
        if(K>256-H) K-=256;     /* Y coordinate may be negative */

        P=XBuf+CV_WIDTH*(CV_HEIGHT-192)/2+(CV_WIDTH-256)/2+CV_WIDTH*Y+L;
        PT=SprGen+((int)(H>8? AT[2]&0xFC:AT[2])<<3)+Y-K-1;
        C=XPal[C];

        /* Mask 1: clip left sprite boundary */
        K=L>=0? 0x0FFFF:(0x10000>>-L)-1;
        /* Mask 2: clip right sprite boundary */
        if(L>256-H) K^=((0x00200>>(H-8))<<(L-257+H))-1;
        /* Get and clip the sprite data */
        K&=((int)PT[0]<<8)|(H>8? PT[16]:0x00);

        /* Draw left 8 pixels of the sprite */
        if(K&0xFF00)
        {
          if(K&0x8000) P[0]=C;if(K&0x4000) P[1]=C;
          if(K&0x2000) P[2]=C;if(K&0x1000) P[3]=C;
          if(K&0x0800) P[4]=C;if(K&0x0400) P[5]=C;
          if(K&0x0200) P[6]=C;if(K&0x0100) P[7]=C;
        }

        /* Draw right 8 pixels of the sprite */
        if(K&0x00FF)
        {
          if(K&0x0080) P[8]=C; if(K&0x0040) P[9]=C;
          if(K&0x0020) P[10]=C;if(K&0x0010) P[11]=C;
          if(K&0x0008) P[12]=C;if(K&0x0004) P[13]=C;
          if(K&0x0002) P[14]=C;if(K&0x0001) P[15]=C;
        }
      }
    }
}

/** RefreshLine0() *******************************************/
/** Refresh line Y (0..191) of SCREEN0, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine0(register byte Y)
{
  register byte X,K,Offset,FC,BC;
  register byte *P,*T;

  P=XBuf+CV_WIDTH*(CV_HEIGHT-192)/2+CV_WIDTH*Y;
  XPal[0]=BGColor? XPal[BGColor]:XPal0;

  if(!ScreenON) memset(P,XPal[BGColor],CV_WIDTH);
  else
  {
    BC=XPal[BGColor];
    FC=XPal[FGColor];
    T=ChrTab+(Y>>3)*40;
    Offset=Y&0x07;

    memset(P,BC,(CV_WIDTH-240)/2);
    P+=(CV_WIDTH-240)/2;

    for(X=0;X<40;X++)
    {
      K=ChrGen[((int)*T<<3)+Offset];
      P[0]=K&0x80? FC:BC;P[1]=K&0x40? FC:BC;
      P[2]=K&0x20? FC:BC;P[3]=K&0x10? FC:BC;
      P[4]=K&0x08? FC:BC;P[5]=K&0x04? FC:BC;
      P+=6;T++;
    }

    memset(P,BC,(CV_WIDTH-240)/2);
  }

  RefreshBorder(Y);
}

/** RefreshLine1() *******************************************/
/** Refresh line Y (0..191) of SCREEN1, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine1(register byte Y)
{
  register byte X,K,Offset,FC,BC;
  register byte *P,*T;

  P=XBuf+CV_WIDTH*(CV_HEIGHT-192)/2+CV_WIDTH*Y;
  XPal[0]=BGColor? XPal[BGColor]:XPal0;

  if(!ScreenON) memset(P,XPal[BGColor],CV_WIDTH);
  else
  {
    T=ChrTab+(Y>>3)*32;
    Offset=Y&0x07;

    memset(P,XPal[BGColor],(CV_WIDTH-256)/2);
    P+=(CV_WIDTH-256)/2;

    for(X=0;X<32;X++)
    {
      K=*T;
      BC=ColTab[K>>3];
      K=ChrGen[((int)K<<3)+Offset];
      FC=XPal[BC>>4];
      BC=XPal[BC&0x0F];
      P[0]=K&0x80? FC:BC;P[1]=K&0x40? FC:BC;
      P[2]=K&0x20? FC:BC;P[3]=K&0x10? FC:BC;
      P[4]=K&0x08? FC:BC;P[5]=K&0x04? FC:BC; 
      P[6]=K&0x02? FC:BC;P[7]=K&0x01? FC:BC;
      P+=8;T++;
    }

    memset(P,XPal[BGColor],(CV_WIDTH-256)/2);
    RefreshSprites(Y);
  }

  RefreshBorder(Y);
}

/** RefreshLine2() *******************************************/
/** Refresh line Y (0..191) of SCREEN2, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine2(register byte Y)
{
  register byte X,K,FC,BC,Offset;
  register byte *P,*T,*PGT,*CLT;
  register int I;

  P=XBuf+CV_WIDTH*(CV_HEIGHT-192)/2+CV_WIDTH*Y;
  XPal[0]=BGColor? XPal[BGColor]:XPal0;

  if(!ScreenON) memset(P,XPal[BGColor],CV_WIDTH);
  else
  {
    I=(int)(Y&0xC0)<<5;
    PGT=ChrGen+I;
    CLT=ColTab+I;
    T=ChrTab+(Y>>3)*32;
    Offset=Y&0x07;

    memset(P,XPal[BGColor],(CV_WIDTH-256)/2);
    P+=(CV_WIDTH-256)/2;

    for(X=0;X<32;X++)
    {
      I=((int)*T<<3)+Offset;
      K=PGT[I];
      BC=CLT[I];
      FC=XPal[BC>>4];
      BC=XPal[BC&0x0F];
      P[0]=K&0x80? FC:BC;P[1]=K&0x40? FC:BC;
      P[2]=K&0x20? FC:BC;P[3]=K&0x10? FC:BC;
      P[4]=K&0x08? FC:BC;P[5]=K&0x04? FC:BC;
      P[6]=K&0x02? FC:BC;P[7]=K&0x01? FC:BC;
      P+=8;T++;
    }

    memset(P,XPal[BGColor],(CV_WIDTH-256)/2);
    RefreshSprites(Y);
  }    

  RefreshBorder(Y);
}

/** RefreshLine3() *******************************************/
/** Refresh line Y (0..191) of SCREEN3, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine3(register byte Y)
{
  register byte X,K,Offset;
  register byte *P,*T;

  P=XBuf+CV_WIDTH*(CV_HEIGHT-192)/2+CV_WIDTH*Y;
  XPal[0]=BGColor? XPal[BGColor]:XPal0;

  if(!ScreenON) memset(P,XPal[BGColor],CV_WIDTH);
  else
  {
    T=ChrTab+(Y>>3)*32;
    Offset=(Y&0x1C)>>2;

    memset(P,XPal[BGColor],(CV_WIDTH-256)/2);
    P+=(CV_WIDTH-256)/2;

    for(X=0;X<32;X++)
    {
      K=ChrGen[((int)*T<<3)+Offset];
      P[0]=P[1]=P[2]=P[3]=XPal[K>>4];
      P[4]=P[5]=P[6]=P[7]=XPal[K&0x0F];
      P+=8;T++;
    }

    memset(P,XPal[BGColor],(CV_WIDTH-256)/2);
    RefreshSprites(Y);
  }

  RefreshBorder(Y);
}
