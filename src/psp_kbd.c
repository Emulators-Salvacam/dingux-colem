/*
 *  Copyright (C) 2009 Ludovic Jacomme (ludovic.jacomme@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>

#include <SDL.h>

#include "Coleco.h"
#include "global.h"
#include "psp_kbd.h"
#include "psp_menu.h"
#include "psp_sdl.h"
#include "psp_danzeff.h"

# define KBD_MIN_ANALOG_TIME  150000
# define KBD_MIN_START_TIME   800000
# define KBD_MAX_EVENT_TIME   500000
# define KBD_MIN_PENDING_TIME 300000
# define KBD_MIN_HOTKEY_TIME  1000000
# define KBD_MIN_DANZEFF_TIME 150000
# define KBD_MIN_COMMAND_TIME 100000
# define KBD_MIN_BATTCHECK_TIME 90000000 
# define KBD_MIN_AUTOFIRE_TIME   1000000

 static gp2xCtrlData    loc_button_data;
 static unsigned int   loc_last_event_time = 0;
 static unsigned int   loc_last_hotkey_time = 0;
 static long           first_time_stamp = -1;
 static long           first_time_auto_stamp = -1;
 static char           loc_button_press[ KBD_MAX_BUTTONS ]; 
 static char           loc_button_release[ KBD_MAX_BUTTONS ]; 
 static unsigned int   loc_button_mask[ KBD_MAX_BUTTONS ] =
 {
   GP2X_CTRL_UP         , /*  KBD_UP         */
   GP2X_CTRL_RIGHT      , /*  KBD_RIGHT      */
   GP2X_CTRL_DOWN       , /*  KBD_DOWN       */
   GP2X_CTRL_LEFT       , /*  KBD_LEFT       */
   GP2X_CTRL_TRIANGLE   , /*  KBD_TRIANGLE   */
   GP2X_CTRL_CIRCLE     , /*  KBD_CIRCLE     */
   GP2X_CTRL_CROSS      , /*  KBD_CROSS      */
   GP2X_CTRL_SQUARE     , /*  KBD_SQUARE     */
   GP2X_CTRL_SELECT     , /*  KBD_SELECT     */
   GP2X_CTRL_START      , /*  KBD_START      */
   GP2X_CTRL_LTRIGGER   , /*  KBD_LTRIGGER   */
   GP2X_CTRL_RTRIGGER   , /*  KBD_RTRIGGER   */
   GP2X_CTRL_FIRE,        /*  KBD_FIRE       */
 };

 static char loc_button_name[ KBD_ALL_BUTTONS ][20] =
 {
   "UP",
   "RIGHT",
   "DOWN",
   "LEFT",
   "Y",      // Triangle
   "B",      // Circle
   "X",      // Cross
   "A",      // Square
   "SELECT",
   "START",
   "LTRIGGER",
   "RTRIGGER",
   "JOY_FIRE",
   "JOY_UP",
   "JOY_RIGHT",
   "JOY_DOWN",
   "JOY_LEFT"
 };

 static char loc_button_name_L[ KBD_ALL_BUTTONS ][20] =
 {
   "L_UP",
   "L_RIGHT",
   "L_DOWN",
   "L_LEFT",
   "L_Y",      // Triangle
   "L_B",      // Circle
   "L_X",      // Cross
   "L_A",      // Square
   "L_SELECT",
   "L_START",
   "L_LTRIGGER",
   "L_RTRIGGER",
   "L_JOY_FIRE",
   "L_JOY_UP",
   "L_JOY_RIGHT",
   "L_JOY_DOWN",
   "L_JOY_LEFT"
 };
 
  static char loc_button_name_R[ KBD_ALL_BUTTONS ][20] =
 {
   "R_UP",
   "R_RIGHT",
   "R_DOWN",
   "R_LEFT",
   "R_Y",      // Triangle
   "R_B",      // Circle
   "R_X",      // Cross
   "R_A",      // Square
   "R_SELECT",
   "R_START",
   "R_LTRIGGER",
   "R_RTRIGGER",
   "R_JOY_FIRE",
   "R_JOY_UP",
   "R_JOY_RIGHT",
   "R_JOY_DOWN",
   "R_JOY_LEFT"
 };

  struct cv_key_trans psp_cv_key_to_name[CV_MAX_KEY]=
  {
    { CV_0,            "0" },
    { CV_1,            "1" },
    { CV_2,            "2" },
    { CV_3,            "3" },
    { CV_4,            "4" },
    { CV_5,            "5" },
    { CV_6,            "6" },
    { CV_7,            "7" },
    { CV_8,            "8" },
    { CV_9,            "9" },
    { CV_ASTERISK,     "*" },
    { CV_POUND,        "POUND" },
    { CV_JOY_FIRE1,    "FIRE1" },
    { CV_JOY_FIRE2,    "FIRE2" },
    { CV_JOY_UP,       "UP" },
    { CV_JOY_DOWN,     "DOWN" },
    { CV_JOY_LEFT,     "LEFT" },
    { CV_JOY_RIGHT,    "RIGHT" },
    { CV_RESET,        "RESET" },
    { CV_USER1,        "USER1" },
    { CV_USER2,        "USER2" },

    { CVC_FPS,      "C_FPS" },
    { CVC_JOY,      "C_JOY" },
    { CVC_RENDER,   "C_RENDER" },
    { CVC_LOAD,     "C_LOAD" },
    { CVC_SAVE,     "C_SAVE" },
    { CVC_RESET,    "C_RESET" },
    { CVC_AUTOFIRE, "C_AUTOFIRE" },
    { CVC_INCFIRE,  "C_INCFIRE" },
    { CVC_DECFIRE,  "C_DECFIRE" },
    { CVC_SCREEN,   "C_SCREEN" }
  };

 static int loc_default_mapping[ KBD_ALL_BUTTONS ] = {
   CV_0               , /*  KBD_UP         */
   CV_2               , /*  KBD_RIGHT      */
   CV_1               , /*  KBD_DOWN       */
   CV_3               , /*  KBD_LEFT       */
   CV_POUND           , /*  KBD_TRIANGLE   */
   CV_JOY_FIRE2       , /*  KBD_CIRCLE     */
   CV_JOY_FIRE1       , /*  KBD_CROSS      */
   CV_ASTERISK        , /*  KBD_SQUARE     */
   -1                 , /*  KBD_SELECT     */
   -1                 , /*  KBD_START      */
   KBD_LTRIGGER_MAPPING  , /*  KBD_LTRIGGER   */
   KBD_RTRIGGER_MAPPING  , /*  KBD_RTRIGGER   */
   CV_JOY_FIRE1       , /*  KBD_FIRE   */
   CV_JOY_UP          , /*  KBD_JOY_UP     */
   CV_JOY_RIGHT       , /*  KBD_JOY_RIGHT  */
   CV_JOY_DOWN        , /*  KBD_JOY_DOWN   */
   CV_JOY_LEFT          /*  KBD_JOY_LEFT   */
  };

 static int loc_default_mapping_L[ KBD_ALL_BUTTONS ] = {
   CV_JOY_UP          , /*  KBD_UP         */
   CV_JOY_RIGHT       , /*  KBD_RIGHT      */
   CV_JOY_DOWN        , /*  KBD_DOWN       */
   CV_JOY_LEFT        , /*  KBD_LEFT       */
   CVC_LOAD           , /*  KBD_TRIANGLE   */
   CVC_RENDER         , /*  KBD_CIRCLE     */
   CVC_SAVE           , /*  KBD_CROSS      */
   CVC_FPS            , /*  KBD_SQUARE     */
   -1                 , /*  KBD_SELECT     */
   -1                 , /*  KBD_START      */
   KBD_LTRIGGER_MAPPING  , /*  KBD_LTRIGGER   */
   KBD_RTRIGGER_MAPPING  , /*  KBD_RTRIGGER   */
   CV_JOY_FIRE1       , /*  KBD_FIRE   */
   CV_0               , /*  KBD_JOY_UP     */
   CV_2               , /*  KBD_JOY_RIGHT  */
   CV_1               , /*  KBD_JOY_DOWN   */
   CV_3                 /*  KBD_JOY_LEFT   */
  };

 static int loc_default_mapping_R[ KBD_ALL_BUTTONS ] = {
   CV_JOY_UP          , /*  KBD_UP         */
   CV_JOY_RIGHT       , /*  KBD_RIGHT      */
   CV_JOY_DOWN        , /*  KBD_DOWN       */
   CV_JOY_LEFT        , /*  KBD_LEFT       */
   CV_4               , /*  KBD_TRIANGLE   */
   CV_JOY_FIRE2       , /*  KBD_CIRCLE     */
   CVC_AUTOFIRE       , /*  KBD_CROSS      */
   CV_5               , /*  KBD_SQUARE     */
   -1                 , /*  KBD_SELECT     */
   -1                 , /*  KBD_START      */
   KBD_LTRIGGER_MAPPING  , /*  KBD_LTRIGGER   */
   KBD_RTRIGGER_MAPPING  , /*  KBD_RTRIGGER   */
   CV_JOY_FIRE1       , /*  KBD_FIRE   */
   CV_0               , /*  KBD_JOY_UP     */
   CVC_INCFIRE        , /*  KBD_JOY_RIGHT  */
   CV_1               , /*  KBD_JOY_DOWN   */
   CVC_DECFIRE          /*  KBD_JOY_LEFT   */
  };

 int psp_kbd_mapping[ KBD_ALL_BUTTONS ];
 int psp_kbd_mapping_L[ KBD_ALL_BUTTONS ];
 int psp_kbd_mapping_R[ KBD_ALL_BUTTONS ];
 int psp_kbd_presses[ KBD_ALL_BUTTONS ];
 int kbd_ltrigger_mapping_active;
 int kbd_rtrigger_mapping_active;

# define KBD_MAX_ENTRIES   21


  int kbd_layout[KBD_MAX_ENTRIES][2] = {
    /* Key            Ascii */
    { CV_0,          '0' },
    { CV_1,          '1' },
    { CV_2,          '2' },
    { CV_3,          '3' },
    { CV_4,          '4' },
    { CV_5,          '5' },
    { CV_6,          '6' },
    { CV_7,          '7' },
    { CV_8,          '8' },
    { CV_9,          '9' },
    { CV_ASTERISK,   '*' },
    { CV_POUND,      '$' },
    { CV_JOY_FIRE1,  -1  },
    { CV_JOY_FIRE2,  -1  },
    { CV_JOY_UP,     -1  },
    { CV_JOY_DOWN,   -1  },
    { CV_JOY_LEFT,   -1  },
    { CV_JOY_RIGHT,  -1  },
    { CV_RESET,      -1  },
    { CV_USER1,      -1  },
    { CV_USER2,      -1  }
  };

        int psp_kbd_mapping[ KBD_ALL_BUTTONS ];

 static int danzeff_cv_key     = 0;
 static int danzeff_cv_pending = 0;
 static int danzeff_mode        = 0;

int
cv_key_event(int cv_idx, int press)
{
  int joy = CV.psp_active_joystick;

  if ((cv_idx >=         0) && 
      (cv_idx <= CV_USER2 )) {

    if (press) {
      if ((cv_idx >= CV_0) && (cv_idx <= CV_9)) {
        JoyState[joy] = (JoyState[joy]&0xFFF0)|(cv_idx - CV_0);
      } else {
        switch( cv_idx ) {
          case CV_JOY_FIRE2 : JoyState[joy] &=0xBFFF; break;
          case CV_JOY_FIRE1 : JoyState[joy] &=0xFFBF; break;
          case CV_JOY_DOWN  : JoyState[joy]&=0xFBFF; break;
          case CV_JOY_UP    : JoyState[joy]&=0xFEFF; break;
          case CV_JOY_LEFT  : JoyState[joy]&=0xF7FF; break;
          case CV_JOY_RIGHT : JoyState[joy]&=0xFDFF; break;
          case CV_ASTERISK  : JoyState[joy]=(JoyState[joy]&0xFFF0)|10;break;
          case CV_POUND     : JoyState[joy]=(JoyState[joy]&0xFFF0)|11;break;
          case CV_USER1     : JoyState[joy]=(JoyState[joy]&0xFFF0)|12;break;
          case CV_USER2     : JoyState[joy]=(JoyState[joy]&0xFFF0)|13;break;
          case CV_RESET     : JoyState[joy]=0xFFFF;break;
        }
      }
    } else {
      if ((cv_idx >= CV_0) && (cv_idx <= CV_9)) {
        if((JoyState[joy]&0x000F)==(cv_idx -CV_0)) JoyState[joy]|=0x000F;
      } else {
        switch( cv_idx ) {
          case CV_JOY_FIRE2 : JoyState[joy]|=0x4000;break;
          case CV_JOY_FIRE1 : JoyState[joy]|=0x0040;break;
          case CV_JOY_DOWN  : JoyState[joy]|=0x0400;break;
          case CV_JOY_UP    : JoyState[joy]|=0x0100;break;
          case CV_JOY_LEFT  : JoyState[joy]|=0x0800;break;
          case CV_JOY_RIGHT : JoyState[joy]|=0x0200;break;
          case CV_ASTERISK  : if((JoyState[joy]&0x000F)==10) JoyState[joy]|=0x000F; break;
          case CV_POUND     : if((JoyState[joy]&0x000F)==11) JoyState[joy]|=0x000F; break;
          case CV_USER1     : if((JoyState[joy]&0x000F)==12) JoyState[joy]|=0x000F; break;
          case CV_USER2     : if((JoyState[joy]&0x000F)==13) JoyState[joy]|=0x000F; break;
          case CV_RESET     : JoyState[joy]=0xFFFF; break;
        }
      }
    }
  } else
  if ((cv_idx >= CVC_FPS) &&
      (cv_idx <= CVC_SCREEN)) {

    if (press) {
      gp2xCtrlData c;
      gp2xCtrlPeekBufferPositive(&c, 1);
      if ((c.TimeStamp - loc_last_hotkey_time) > KBD_MIN_HOTKEY_TIME) {
        loc_last_hotkey_time = c.TimeStamp;
        cv_treat_command_key(cv_idx);
      }
    }
  }
  return 0;
}
int 
cv_kbd_reset()
{
  memset(loc_button_press  , 0, sizeof(loc_button_press));
  memset(loc_button_release, 0, sizeof(loc_button_release));

  JoyState[0] = 0xFFFF;
  JoyState[1] = 0xFFFF;

  return 0;
}

int
cv_get_key_from_ascii(int key_ascii)
{
  int index;
  for (index = 0; index < KBD_MAX_ENTRIES; index++) {
   if (kbd_layout[index][1] == key_ascii) return kbd_layout[index][0];
  }
  return -1;
}

void
psp_kbd_default_settings()
{
  memcpy(psp_kbd_mapping, loc_default_mapping, sizeof(loc_default_mapping));
  memcpy(psp_kbd_mapping_L, loc_default_mapping_L, sizeof(loc_default_mapping_L));
  memcpy(psp_kbd_mapping_R, loc_default_mapping_R, sizeof(loc_default_mapping_R));
}

int
psp_kbd_reset_hotkeys(void)
{
  int index;
  int key_id;
  for (index = 0; index < KBD_ALL_BUTTONS; index++) {
    key_id = loc_default_mapping[index];
    if ((key_id >= CVC_FPS) && (key_id <= CVC_SCREEN)) {
      psp_kbd_mapping[index] = key_id;
    }
    key_id = loc_default_mapping_L[index];
    if ((key_id >= CVC_FPS) && (key_id <= CVC_SCREEN)) {
      psp_kbd_mapping_L[index] = key_id;
    }
    key_id = loc_default_mapping_R[index];
    if ((key_id >= CVC_FPS) && (key_id <= CVC_SCREEN)) {
      psp_kbd_mapping_R[index] = key_id;
    }
  }
  return 0;
}

int
psp_kbd_load_mapping_file(FILE *KbdFile)
{
  char     Buffer[512];
  char    *Scan;
  int      tmp_mapping[KBD_ALL_BUTTONS];
  int      tmp_mapping_L[KBD_ALL_BUTTONS];
  int      tmp_mapping_R[KBD_ALL_BUTTONS];
  int      cv_key_id = 0;
  int      kbd_id = 0;

  memcpy(tmp_mapping, loc_default_mapping, sizeof(loc_default_mapping));
  memcpy(tmp_mapping_L, loc_default_mapping_L, sizeof(loc_default_mapping_R));
  memcpy(tmp_mapping_R, loc_default_mapping_R, sizeof(loc_default_mapping_R));

  while (fgets(Buffer,512,KbdFile) != (char *)0) {
      
      Scan = strchr(Buffer,'\n');
      if (Scan) *Scan = '\0';
      /* For this #@$% of windows ! */
      Scan = strchr(Buffer,'\r');
      if (Scan) *Scan = '\0';
      if (Buffer[0] == '#') continue;

      Scan = strchr(Buffer,'=');
      if (! Scan) continue;
    
      *Scan = '\0';
      cv_key_id = atoi(Scan + 1);

      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,loc_button_name[kbd_id])) {
          tmp_mapping[kbd_id] = cv_key_id;
          //break;
        }
      }
      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,loc_button_name_L[kbd_id])) {
          tmp_mapping_L[kbd_id] = cv_key_id;
          //break;
        }
      }
      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,loc_button_name_R[kbd_id])) {
          tmp_mapping_R[kbd_id] = cv_key_id;
          //break;
        }
      }
  }

  memcpy(psp_kbd_mapping, tmp_mapping, sizeof(psp_kbd_mapping));
  memcpy(psp_kbd_mapping_L, tmp_mapping_L, sizeof(psp_kbd_mapping_L));
  memcpy(psp_kbd_mapping_R, tmp_mapping_R, sizeof(psp_kbd_mapping_R));
  
  return 0;
}

int
psp_kbd_load_mapping(char *kbd_filename)
{
  FILE    *KbdFile;
  int      error = 0;
  
  KbdFile = fopen(kbd_filename, "r");
  error   = 1;

  if (KbdFile != (FILE*)0) {
  psp_kbd_load_mapping_file(KbdFile);
  error = 0;
    fclose(KbdFile);
  }

  kbd_ltrigger_mapping_active = 0;
  kbd_rtrigger_mapping_active = 0;
    
  return error;
}

int
psp_kbd_save_mapping(char *kbd_filename)
{
  FILE    *KbdFile;
  int      kbd_id = 0;
  int      error = 0;

  KbdFile = fopen(kbd_filename, "w");
  error   = 1;

  if (KbdFile != (FILE*)0) {

    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", loc_button_name[kbd_id], psp_kbd_mapping[kbd_id]);
    }
    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", loc_button_name_L[kbd_id], psp_kbd_mapping_L[kbd_id]);
    }
    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", loc_button_name_R[kbd_id], psp_kbd_mapping_R[kbd_id]);
    }
    error = 0;
    fclose(KbdFile);
  }

  return error;
}

int 
psp_kbd_is_danzeff_mode()
{
  return danzeff_mode;
}

int
psp_kbd_enter_danzeff()
{
  unsigned int danzeff_key = 0;
  int          cv_key     = 0;
  int          key_event   = 0;
  gp2xCtrlData  c;

  if (! danzeff_mode) {
    psp_init_keyboard();
    danzeff_mode = 1;
  }

  gp2xCtrlPeekBufferPositive(&c, 1);

  if (danzeff_cv_pending) 
  {
    if ((c.TimeStamp - loc_last_event_time) > KBD_MIN_PENDING_TIME) {
      loc_last_event_time = c.TimeStamp;
      danzeff_cv_pending = 0;
      cv_key_event(danzeff_cv_key, 0);
    }

    return 0;
  }

  if ((c.TimeStamp - loc_last_event_time) > KBD_MIN_DANZEFF_TIME) {
    loc_last_event_time = c.TimeStamp;
  
    gp2xCtrlPeekBufferPositive(&c, 1);
    danzeff_key = danzeff_readInput(c);
  }

  if (danzeff_key > DANZEFF_START) {
    cv_key = cv_get_key_from_ascii(danzeff_key);

    if (cv_key != -1) {
      danzeff_cv_key     = cv_key;
      danzeff_cv_pending = 1;
      cv_key_event(danzeff_cv_key, 1);
    }

    return 1;

  } else if (danzeff_key == DANZEFF_START) {
    danzeff_mode       = 0;
    danzeff_cv_pending = 0;
    danzeff_cv_key     = 0;

    psp_kbd_wait_no_button();

  } else if (danzeff_key == DANZEFF_SELECT) {
    danzeff_mode       = 0;
    danzeff_cv_pending = 0;
    danzeff_cv_key     = 0;
    psp_main_menu();
    psp_init_keyboard();

    psp_kbd_wait_no_button();
  }

  return 0;
}

int
cv_decode_key(int psp_b, int button_pressed)
{
  int wake = 0;
  int reverse_cursor = ! CV.psp_reverse_analog;

  if (reverse_cursor) {
    if ((psp_b >= KBD_JOY_UP  ) &&
        (psp_b <= KBD_JOY_LEFT)) {
      psp_b = psp_b - KBD_JOY_UP + KBD_UP;
    } else
    if ((psp_b >= KBD_UP  ) &&
        (psp_b <= KBD_LEFT)) {
      psp_b = psp_b - KBD_UP + KBD_JOY_UP;
    }
  }

  if (psp_b == KBD_START) {
     if (button_pressed) psp_kbd_enter_danzeff();
  } else
  if (psp_b == KBD_SELECT) {
    if (button_pressed) {
      psp_main_menu();
      psp_init_keyboard();
    }
  } else {
 
    if (psp_kbd_mapping[psp_b] >= 0) {
      wake = 1;
      if (button_pressed) {
        // Determine which buton to press first (ie which mapping is currently active)
        if (kbd_ltrigger_mapping_active) {
          // Use ltrigger mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping_L[psp_b];
          cv_key_event(psp_kbd_presses[psp_b], button_pressed);
        } else
        if (kbd_rtrigger_mapping_active) {
          // Use rtrigger mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping_R[psp_b];
          cv_key_event(psp_kbd_presses[psp_b], button_pressed);
        } else {
          // Use standard mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping[psp_b];
          cv_key_event(psp_kbd_presses[psp_b], button_pressed);
        }
      } else {
          // Determine which button to release (ie what was pressed before)
          cv_key_event(psp_kbd_presses[psp_b], button_pressed);
      }

    } else {
      if (psp_kbd_mapping[psp_b] == KBD_LTRIGGER_MAPPING) {
        kbd_ltrigger_mapping_active = button_pressed;
        kbd_rtrigger_mapping_active = 0;
      } else
      if (psp_kbd_mapping[psp_b] == KBD_RTRIGGER_MAPPING) {
        kbd_rtrigger_mapping_active = button_pressed;
        kbd_ltrigger_mapping_active = 0;
      }
    }
  }
  return 0;
}

void
kbd_change_auto_fire(int auto_fire)
{
  CV.cv_auto_fire = auto_fire;
  if (CV.cv_auto_fire_pressed) {
    if (CV.psp_active_joystick) {
      cv_key_event(CV_JOY_FIRE2, 0);
    } else {
      cv_key_event(CV_JOY_FIRE1, 0);
    }
    CV.cv_auto_fire_pressed = 0;
  }
}


static int 
kbd_reset_button_status(void)
{
  int b = 0;
  /* Reset Button status */
  for (b = 0; b < KBD_MAX_BUTTONS; b++) {
    loc_button_press[b]   = 0;
    loc_button_release[b] = 0;
  }
  psp_init_keyboard();
  return 0;
}

int
kbd_scan_keyboard(void)
{
  gp2xCtrlData c;
  long        delta_stamp;
  int         event;
  int         b;

  event = 0;
  gp2xCtrlPeekBufferPositive( &c, 1 );

  if (CV.cv_auto_fire) {
    delta_stamp = c.TimeStamp - first_time_auto_stamp;
    if ((delta_stamp < 0) || 
        (delta_stamp > (KBD_MIN_AUTOFIRE_TIME / (1 + CV.cv_auto_fire_period)))) {
      first_time_auto_stamp = c.TimeStamp;

      if (CV.psp_active_joystick) {
        cv_key_event(CV_JOY_FIRE2, CV.cv_auto_fire_pressed);
      } else {
        cv_key_event(CV_JOY_FIRE1, CV.cv_auto_fire_pressed);
      }
      CV.cv_auto_fire_pressed = ! CV.cv_auto_fire_pressed;
    }
  }

  for (b = 0; b < KBD_MAX_BUTTONS; b++) 
  {
    if (c.Buttons & loc_button_mask[b]) {
      if (!(loc_button_data.Buttons & loc_button_mask[b])) {
        loc_button_press[b] = 1;
        event = 1;
      }
    } else {
      if (loc_button_data.Buttons & loc_button_mask[b]) {
        loc_button_release[b] = 1;
        loc_button_press[b] = 0;
        event = 1;
      }
    }
  }
  memcpy(&loc_button_data,&c,sizeof(gp2xCtrlData));

  return event;
}

void
kbd_wait_start(void)
{
  while (1)
  {
    gp2xCtrlData c;
    gp2xCtrlReadBufferPositive(&c, 1);
    c.Buttons &= PSP_ALL_BUTTON_MASK;
    if (c.Buttons & GP2X_CTRL_START) break;
  }
  psp_kbd_wait_no_button();
}

void
psp_init_keyboard(void)
{
  cv_kbd_reset();
  kbd_ltrigger_mapping_active = 0;
  kbd_rtrigger_mapping_active = 0;
}

void
psp_kbd_wait_no_button(void)
{
  gp2xCtrlData c;

  do {
   gp2xCtrlPeekBufferPositive(&c, 1);
   c.Buttons &= PSP_ALL_BUTTON_MASK;
  } while (c.Buttons != 0);
} 

void
psp_kbd_wait_button(void)
{
  gp2xCtrlData c;

  do {
   gp2xCtrlReadBufferPositive(&c, 1);
   c.Buttons &= PSP_ALL_BUTTON_MASK;
  } while (c.Buttons == 0);
} 

int
psp_update_keys(void)
{
  int         b;

  static char first_time = 1;
  static int release_pending = 0;

  if (first_time) {

    gp2xCtrlData c;
    gp2xCtrlPeekBufferPositive(&c, 1);
    c.Buttons &= PSP_ALL_BUTTON_MASK;

    if (first_time_stamp == -1) first_time_stamp = c.TimeStamp;

    first_time      = 0;
    release_pending = 0;

    for (b = 0; b < KBD_MAX_BUTTONS; b++) {
      loc_button_release[b] = 0;
      loc_button_press[b] = 0;
    }
    gp2xCtrlPeekBufferPositive(&loc_button_data, 1);
    loc_button_data.Buttons &= PSP_ALL_BUTTON_MASK;

    psp_main_menu();
    psp_init_keyboard();

    return 0;
  }

  cv_apply_cheats();

  if (danzeff_mode) {
    return psp_kbd_enter_danzeff();
  }

  if (release_pending)
  {
    release_pending = 0;
    for (b = 0; b < KBD_MAX_BUTTONS; b++) {
      if (loc_button_release[b]) {
        loc_button_release[b] = 0;
        loc_button_press[b] = 0;
        cv_decode_key(b, 0);
      }
    }
  }

  kbd_scan_keyboard();

  /* check press event */
  for (b = 0; b < KBD_MAX_BUTTONS; b++) {
    if (loc_button_press[b]) {
      loc_button_press[b] = 0;
      release_pending     = 0;
      cv_decode_key(b, 1);
    }
  }
  /* check release event */
  for (b = 0; b < KBD_MAX_BUTTONS; b++) {
    if (loc_button_release[b]) {
      release_pending = 1;
      break;
    }
  }

  return 0;
}
