/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                        Coleco.c                         **/
/**                                                         **/
/** This file contains implementation for the Coleco-       **/
/** specific hardware: VDP, etc. Initialization code and    **/
/** definitions needed for the machine-dependent drivers    **/
/** are also here.                                          **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <zlib.h>

#include "global.h"
#include "Coleco.h"
#include "psp_fmgr.h"
#include "psp_sdl.h"
#include "psp_kbd.h"
#include "miniunz.h"

//LUDO/ZX: 3.58 MHz for NTSC
// Z80 freq  3.58 Mhz
// 60 fps : 59666 cycles / frames
// 50 fps : 71600 cycles / frames

byte UPeriod = 1;
int  VPeriod = 59666;
int  HPeriod = 233;

//byte UPeriod     = 2;          /* Interrupts/scr. update        */
//int  VPeriod     = 60000;      /* Number of cycles per VBlank   */
//int  HPeriod     = 215;        /* Number of cycles per HBlank   */
//PREV: byte UPeriod     = 4;          /* Interrupts/scr. update        */
//PREV: int  VPeriod     = 80000;      /* Number of cycles per VBlank   */
//PREV: int  HPeriod     = 286;        /* Number of cycles per HBlank   */

byte LogSnd = 0;       /* 1: Log soundtrack into a file */
byte Adam   = 0;       /* 1: Emulate Coleco Adam        */

byte *VRAM;
byte *RAM;             /* Main and Video RAMs           */
Z80 CPU;               /* Z80 CPU registers and state   */
SN76489 PSG;           /* SN76489 PSG state             */

byte ExitNow;          /* 1: Exit the emulator          */

byte JoyMode;          /* Joystick controller mode      */
word JoyState[2];      /* Joystick states               */

char *PrnName = NULL;  /* Printer redirection file      */
FILE *PrnStream;

byte *ChrGen;
byte *ChrTab;
byte *ColTab;                  /* VDP tables (screens)          */
byte *SprGen;
byte *SprTab;                  /* VDP tables (sprites)          */
pair WVAddr;
pair RVAddr;                   /* Storage for VRAM addresses    */
byte VKey;                     /* VDP address latch key         */
byte FGColor;
byte BGColor;          /* Colors                        */
byte ScrMode;                  /* Current screen mode           */
byte CurLine;                  /* Current scanline              */
byte VDP[8];
byte VDPStatus;         /* VDP registers                 */

/*** TMS9918/9928 Palette *******************************************/
struct { byte R,G,B; } Palette[16] =
{
  {0x00,0x00,0x00},{0x00,0x00,0x00},{0x20,0xC0,0x20},{0x60,0xE0,0x60},
  {0x20,0x20,0xE0},{0x40,0x60,0xE0},{0xA0,0x20,0x20},{0x40,0xC0,0xE0},
  {0xE0,0x20,0x20},{0xE0,0x60,0x60},{0xC0,0xC0,0x20},{0xC0,0xC0,0x80},
  {0x20,0x80,0x20},{0xC0,0x40,0xA0},{0xA0,0xA0,0xA0},{0xE0,0xE0,0xE0}
};

unsigned short Palette_rgb[16];

/*** Screen handlers and masks for VDP table address registers ******/
struct
{
  void (*Refresh)(byte Y);
  byte R2,R3,R4,R5;
} SCR[MAXSCREEN+1] =
{
  { RefreshLine0,0x7F,0x00,0x3F,0x00 },   /* SCREEN 0:TEXT 40x24    */
  { RefreshLine1,0x7F,0xFF,0x3F,0xFF },   /* SCREEN 1:TEXT 32x24    */
  { RefreshLine2,0x7F,0x80,0x3C,0xFF },   /* SCREEN 2:BLOCK 256x192 */
  { RefreshLine3,0x7F,0x00,0x3F,0xFF }    /* SCREEN 3:GFX 64x48x16  */
};

static void VDPOut(byte Reg,byte Value); /* Write value into VDP */
static void CheckSprites(void);          /* Collisions/5th spr.  */

extern void Play(int C,int F,int V);     /* Log and play sound   */

/** StartSMS() ***********************************************/
/** Allocate memory, load ROM image, initialize hardware,   **/
/** CPU and start the emulation. This function returns 0 in **/
/** the case of failure.                                    **/
/*************************************************************/

//LUDO:
  ColecoVision_t CV;

void
cv_update_save_name(char *Name)
{
  char        TmpFileName[MAX_PATH];
  struct stat aStat;
  int         index;
  char       *SaveName;
  char       *Scan1;
  char       *Scan2;

  SaveName = strrchr(Name,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = Name;

  if (!strncasecmp(SaveName, "sav_", 4)) {
    Scan1 = SaveName + 4;
    Scan2 = strrchr(Scan1, '_');
    if (Scan2 && (Scan2[1] >= '0') && (Scan2[1] <= '5')) {
      strncpy(CV.cv_save_name, Scan1, MAX_PATH);
      CV.cv_save_name[Scan2 - Scan1] = '\0';
    } else {
      strncpy(CV.cv_save_name, SaveName, MAX_PATH);
    }
  } else {
    strncpy(CV.cv_save_name, SaveName, MAX_PATH);
  }

  if (CV.cv_save_name[0] == '\0') {
    strcpy(CV.cv_save_name,"default");
  }

  for (index = 0; index < CV_MAX_SAVE_STATE; index++) {
    CV.cv_save_state[index].used  = 0;
    memset(&CV.cv_save_state[index].date, 0, sizeof(time_t));
    CV.cv_save_state[index].thumb = 0;

    snprintf(TmpFileName, MAX_PATH, "%s/save/sav_%s_%d.stz", CV.cv_home_dir, CV.cv_save_name, index);
    if (! stat(TmpFileName, &aStat)) {
      CV.cv_save_state[index].used = 1;
      CV.cv_save_state[index].date = aStat.st_mtime;
      snprintf(TmpFileName, MAX_PATH, "%s/save/sav_%s_%d.png", CV.cv_home_dir, CV.cv_save_name, index);
      if (! stat(TmpFileName, &aStat)) {
        if (psp_sdl_load_thumb_png(CV.cv_save_state[index].surface, TmpFileName)) {
          CV.cv_save_state[index].thumb = 1;
        }
      }
    }
  }

  CV.comment_present = 0;
  snprintf(TmpFileName, MAX_PATH, "%s/txt/%s.txt", CV.cv_home_dir, CV.cv_save_name);
  if (! stat(TmpFileName, &aStat))  {
    CV.comment_present = 1;
  }
}

void
reset_save_name()
{
  cv_update_save_name("");
}

typedef struct thumb_list {
  struct thumb_list *next;
  char              *name;
  char              *thumb;
} thumb_list;

static thumb_list* loc_head_thumb = 0;

static void
loc_del_thumb_list()
{
  while (loc_head_thumb != 0) {
    thumb_list *del_elem = loc_head_thumb;
    loc_head_thumb = loc_head_thumb->next;
    if (del_elem->name) free( del_elem->name );
    if (del_elem->thumb) free( del_elem->thumb );
    free(del_elem);
  }
}

static void
loc_add_thumb_list(char* filename)
{
  thumb_list *new_elem;
  char tmp_filename[MAX_PATH];

  strcpy(tmp_filename, filename);
  char* save_name = tmp_filename;

  /* .png extention */
  char* Scan = strrchr(save_name, '.');
  if ((! Scan) || (strcasecmp(Scan, ".png"))) return;
  *Scan = 0;

  if (strncasecmp(save_name, "sav_", 4)) return;
  save_name += 4;

  Scan = strrchr(save_name, '_');
  if (! Scan) return;
  *Scan = 0;

  /* only one png for a given save name */
  new_elem = loc_head_thumb;
  while (new_elem != 0) {
    if (! strcasecmp(new_elem->name, save_name)) return;
    new_elem = new_elem->next;
  }

  new_elem = (thumb_list *)malloc( sizeof( thumb_list ) );
  new_elem->next = loc_head_thumb;
  loc_head_thumb = new_elem;
  new_elem->name  = strdup( save_name );
  new_elem->thumb = strdup( filename );
}

void
load_thumb_list()
{
  char SaveDirName[MAX_PATH];
  DIR* fd = 0;

  loc_del_thumb_list();

  snprintf(SaveDirName, MAX_PATH, "%s/save", CV.cv_home_dir);

  fd = opendir(SaveDirName);
  if (!fd) return;

  struct dirent *a_dirent;
  while ((a_dirent = readdir(fd)) != 0) {
    if(a_dirent->d_name[0] == '.') continue;
    if (a_dirent->d_type != DT_DIR) 
    {
      loc_add_thumb_list( a_dirent->d_name );
    }
  }
  closedir(fd);
}

int
load_thumb_if_exists(char *Name)
{
  char        FileName[MAX_PATH];
  char        ThumbFileName[MAX_PATH];
  struct stat aStat;
  char       *SaveName;
  char       *Scan;

  strcpy(FileName, Name);
  SaveName = strrchr(FileName,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = FileName;

  Scan = strrchr(SaveName,'.');
  if (Scan) *Scan = '\0';

  if (!SaveName[0]) return 0;

  thumb_list *scan_list = loc_head_thumb;
  while (scan_list != 0) {
    if (! strcasecmp( SaveName, scan_list->name)) {
      snprintf(ThumbFileName, MAX_PATH, "%s/save/%s", CV.cv_home_dir, scan_list->thumb);
      if (! stat(ThumbFileName, &aStat)) 
      {
        if (psp_sdl_load_thumb_png(save_surface, ThumbFileName)) {
          return 1;
        }
      }
    }
    scan_list = scan_list->next;
  }
  return 0;
}

typedef struct comment_list {
  struct comment_list *next;
  char              *name;
  char              *filename;
} comment_list;

static comment_list* loc_head_comment = 0;

static void
loc_del_comment_list()
{
  while (loc_head_comment != 0) {
    comment_list *del_elem = loc_head_comment;
    loc_head_comment = loc_head_comment->next;
    if (del_elem->name) free( del_elem->name );
    if (del_elem->filename) free( del_elem->filename );
    free(del_elem);
  }
}

static void
loc_add_comment_list(char* filename)
{
  comment_list *new_elem;
  char  tmp_filename[MAX_PATH];

  strcpy(tmp_filename, filename);
  char* save_name = tmp_filename;

  /* .png extention */
  char* Scan = strrchr(save_name, '.');
  if ((! Scan) || (strcasecmp(Scan, ".txt"))) return;
  *Scan = 0;

  /* only one txt for a given save name */
  new_elem = loc_head_comment;
  while (new_elem != 0) {
    if (! strcasecmp(new_elem->name, save_name)) return;
    new_elem = new_elem->next;
  }

  new_elem = (comment_list *)malloc( sizeof( comment_list ) );
  new_elem->next = loc_head_comment;
  loc_head_comment = new_elem;
  new_elem->name  = strdup( save_name );
  new_elem->filename = strdup( filename );
}

void
load_comment_list()
{
  char SaveDirName[MAX_PATH];
  DIR* fd = 0;

  loc_del_comment_list();

  snprintf(SaveDirName, MAX_PATH, "%s/txt", CV.cv_home_dir);

  fd = opendir(SaveDirName);
  if (!fd) return;

  struct dirent *a_dirent;
  while ((a_dirent = readdir(fd)) != 0) {
    if(a_dirent->d_name[0] == '.') continue;
    if (a_dirent->d_type != DT_DIR) 
    {
      loc_add_comment_list( a_dirent->d_name );
    }
  }
  closedir(fd);
}

char*
load_comment_if_exists(char *Name)
{
static char loc_comment_buffer[128];

  char        FileName[MAX_PATH];
  char        TmpFileName[MAX_PATH];
  FILE       *a_file;
  char       *SaveName;
  char       *Scan;

  loc_comment_buffer[0] = 0;

  strcpy(FileName, Name);
  SaveName = strrchr(FileName,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = FileName;

  Scan = strrchr(SaveName,'.');
  if (Scan) *Scan = '\0';

  if (!SaveName[0]) return 0;

  comment_list *scan_list = loc_head_comment;
  while (scan_list != 0) {
    if (! strcasecmp( SaveName, scan_list->name)) {
      snprintf(TmpFileName, MAX_PATH, "%s/txt/%s", CV.cv_home_dir, scan_list->filename);
      a_file = fopen(TmpFileName, "r");
      if (a_file) {
        char* a_scan = 0;
        loc_comment_buffer[0] = 0;
        if (fgets(loc_comment_buffer, 60, a_file) != 0) {
          a_scan = strchr(loc_comment_buffer, '\n');
          if (a_scan) *a_scan = '\0';
          /* For this #@$% of windows ! */
          a_scan = strchr(loc_comment_buffer,'\r');
          if (a_scan) *a_scan = '\0';
          fclose(a_file);
          return loc_comment_buffer;
        }
        fclose(a_file);
        return 0;
      }
    }
    scan_list = scan_list->next;
  }
  return 0;
}

void
cv_kbd_load(void)
{
  char        TmpFileName[MAX_PATH + 1];
  struct stat aStat;

  snprintf(TmpFileName, MAX_PATH, "%s/kbd/%s.kbd", CV.cv_home_dir, CV.cv_save_name );
  if (! stat(TmpFileName, &aStat)) {
    psp_kbd_load_mapping(TmpFileName);
  }
}

int
cv_kbd_save(void)
{
  char TmpFileName[MAX_PATH + 1];
  snprintf(TmpFileName, MAX_PATH, "%s/kbd/%s.kbd", CV.cv_home_dir, CV.cv_save_name );
  return( psp_kbd_save_mapping(TmpFileName) );
}

void
cv_joy_load(void)
{
  char        TmpFileName[MAX_PATH + 1];
  struct stat aStat;

  snprintf(TmpFileName, MAX_PATH, "%s/joy/%s.joy", CV.cv_home_dir, CV.cv_save_name );
  if (! stat(TmpFileName, &aStat)) {
    psp_joy_load_settings(TmpFileName);
  }
}

int
cv_joy_save(void)
{
  char TmpFileName[MAX_PATH + 1];
  snprintf(TmpFileName, MAX_PATH, "%s/joy/%s.joy", CV.cv_home_dir, CV.cv_save_name );
  return( psp_joy_save_settings(TmpFileName) );
}

/*** VDP control register states: ***/
static byte VDPInit[8] = { 0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00 };

void
emulator_reset(void)
{
  /* Initialize VDP registers */
  memcpy(VDP,VDPInit,sizeof(VDP));

  /* Initialize internal variables */
  VKey=1;                              /* VDP address latch key */
  VDPStatus=0x9F;                      /* VDP status register   */
  FGColor=BGColor=0;                   /* Fore/Background color */
  ScrMode=0;                           /* Current screenmode    */
  CurLine=0;                           /* Current scanline      */
  ChrTab=ColTab=ChrGen=VRAM;           /* VDP tables (screen)   */
  SprTab=SprGen=VRAM;                  /* VDP tables (sprites)  */
  JoyMode=0;                           /* Joystick mode key     */
  JoyState[0]=JoyState[1]=0xFFFF;      /* Joystick states       */
  Reset76489(&PSG,Play);               /* Reset SN76489 PSG     */
  Sync76489(&PSG,PSG_SYNC);            /* Make it synchronous   */
  ResetZ80(&CPU);                      /* Reset Z80 registers   */
}

void
myPowerSetClockFrequency(int cpu_clock)
{
  if (CV.cv_current_clock != cpu_clock) {
    gp2xPowerSetClockFrequency(cpu_clock);
    CV.cv_current_clock = cpu_clock;
  }
}

void
cv_default_settings()
{
  //LUDO:
  CV.cv_snd_enable       = 1;
  CV.cv_render_mode      = CV_RENDER_NORMAL;

  CV.cv_ntsc             = 1;
  CV.cv_speed_limiter    = 60;
  CV.cv_auto_fire         = 0;
  CV.cv_auto_fire_period  = 6;
  CV.cv_auto_fire_pressed = 0;
  CV.psp_reverse_analog  = 0;
  CV.psp_cpu_clock        = GP2X_DEF_CLOCK;
  CV.psp_screenshot_id    = 0;
  CV.cv_view_fps          = 0;
  CV.psp_active_joystick  = 0;

  cv_set_video_mode(CV.cv_ntsc);
  myPowerSetClockFrequency(CV.psp_cpu_clock);
}

int
loc_cv_save_settings(char *chFileName)
{
  FILE* FileDesc;
  int   error = 0;

  FileDesc = fopen(chFileName, "w");
  if (FileDesc != (FILE *)0 ) {

    fprintf(FileDesc, "psp_cpu_clock=%d\n"      , CV.psp_cpu_clock);
    fprintf(FileDesc, "psp_reverse_analog=%d\n" , CV.psp_reverse_analog);
    fprintf(FileDesc, "psp_skip_max_frame=%d\n" , CV.psp_skip_max_frame);
    fprintf(FileDesc, "cv_view_fps=%d\n"        , CV.cv_view_fps);
    fprintf(FileDesc, "cv_snd_enable=%d\n"      , CV.cv_snd_enable);
    fprintf(FileDesc, "cv_render_mode=%d\n"     , CV.cv_render_mode);
    fprintf(FileDesc, "cv_vsync=%d\n"           , CV.cv_vsync);
    fprintf(FileDesc, "cv_ntsc=%d\n"            , CV.cv_ntsc);
    fprintf(FileDesc, "cv_speed_limiter=%d\n"   , CV.cv_speed_limiter);
    fprintf(FileDesc, "cv_auto_fire_period=%d\n", CV.cv_auto_fire_period);

    fclose(FileDesc);

  } else {
    error = 1;
  }

  return error;
}

int
cv_save_settings(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/set/%s.set", CV.cv_home_dir, CV.cv_save_name);
  error = loc_cv_save_settings(FileName);

  return error;
}

static int
loc_cv_load_settings(char *chFileName)
{
  char  Buffer[512];
  char *Scan;
  unsigned int Value;
  FILE* FileDesc;

  FileDesc = fopen(chFileName, "r");
  if (FileDesc == (FILE *)0 ) return 0;

  while (fgets(Buffer,512, FileDesc) != (char *)0) {

    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    Scan = strchr(Buffer,'=');
    if (! Scan) continue;

    *Scan = '\0';
    Value = atoi(Scan+1);

    if (!strcasecmp(Buffer,"psp_cpu_clock"))      CV.psp_cpu_clock = Value;
    else
    if (!strcasecmp(Buffer,"cv_view_fps"))     CV.cv_view_fps = Value;
    else
    if (!strcasecmp(Buffer,"psp_reverse_analog")) CV.psp_reverse_analog = Value;
    else
    if (!strcasecmp(Buffer,"psp_skip_max_frame")) CV.psp_skip_max_frame = Value;
    else
    if (!strcasecmp(Buffer,"cv_snd_enable"))      CV.cv_snd_enable = Value;
    else
    if (!strcasecmp(Buffer,"cv_render_mode"))     CV.cv_render_mode = Value;
    else
    if (!strcasecmp(Buffer,"cv_vsync"))     CV.cv_vsync = Value;
    else
    if (!strcasecmp(Buffer,"cv_ntsc"))     CV.cv_ntsc = Value;
    else
    if (!strcasecmp(Buffer,"cv_speed_limiter"))  CV.cv_speed_limiter = Value;
    else
    if (!strcasecmp(Buffer,"cv_auto_fire_period"))  CV.cv_auto_fire_period = Value;
  }

  fclose(FileDesc);

  cv_set_video_mode( CV.cv_ntsc);
  myPowerSetClockFrequency(CV.psp_cpu_clock);

  return 0;
}

void
cv_set_video_mode( int ntsc )
{
  if (ntsc) {
    CV.cv_ntsc = 1;
    VPeriod = 59666;
    HPeriod = 233;
  } else {
    CV.cv_ntsc = 0;
    VPeriod = 71600;
    HPeriod = 280;
  }
}

int
cv_load_settings()
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/set/%s.set", CV.cv_home_dir, CV.cv_save_name);
  error = loc_cv_load_settings(FileName);

  return error;
}

int
cv_load_file_settings(char *FileName)
{
  return loc_cv_load_settings(FileName);
}


static int 
loc_load_rom(char *TmpName)
{
  FILE* F;
  int I;
  int J;

  if(F=fopen(TmpName,"rb"))
  {
    if(fread(RAM+0x8000,1,2,F)!=2) {
      fclose(F);
      return 0;
    }
    else
    {
      I=RAM[0x8000];J=RAM[0x8001];
      if(((I==0x55)&&(J==0xAA))||((I==0xAA)&&(J==0x55)))
      {
        J=2+fread(RAM+0x8002,1,0x7FFE,F);
        if(J<0x1000) {
          fclose(F);
          return 0;
        }
      }
      else {
        fclose(F);
        return 0;
      }
    }
    fclose(F);
  }
  else {
    return 0;
  }
  return 1;
}

static int 
loc_load_rom_buffer(char *rom_buffer, int rom_size )
{
  int I;
  int J;

  if (rom_size <= 0x1000) return 0;

  RAM[0x8000] = rom_buffer[0];
  RAM[0x8001] = rom_buffer[1];
  I=RAM[0x8000];
  J=RAM[0x8001];

  if(((I==0x55)&&(J==0xAA))||((I==0xAA)&&(J==0x55)))
  {
    int mem_len = rom_size-2;
    if (mem_len > 0x7FFE) mem_len = 0x7FFE;

    memcpy(RAM+0x8002,rom_buffer+2, mem_len);
    return 1;
  }
  return 0;
}


int 
StartColeco()
{
  FILE *F;
  int *T,I,J;
  char TmpName[256];

  /* Calculate IPeriod from VPeriod */
  if(UPeriod<1) UPeriod=1;
  if(VPeriod/HPeriod<256) VPeriod=256*HPeriod;
  CPU.IPeriod=HPeriod;
  CPU.TrapBadOps=0;
  CPU.IAutoReset=0;

  /* Zero everything */
  VRAM=RAM=NULL;ExitNow=0;

  if(!(RAM=malloc(0x10000))) { return(0); }
  memset(RAM,NORAM,0x10000);
  if(!(VRAM=malloc(0x4000))) { return(0); }
  memset(VRAM,NORAM,0x4000);

  strcpy(TmpName, CV.cv_home_dir);
  strcat(TmpName,"/coleco.rom");

  if (F=fopen(TmpName,"rb"))
  {
    if(fread(RAM,1,0x2000,F)!=0x2000) {
      fclose(F);
      return 0;
    }
    fclose(F);
  }
  else {
    return 0;
  }

  strcpy(TmpName, CV.cv_home_dir);
  strcat(TmpName,"/default.rom");

  if (! loc_load_rom(TmpName)) return 0;

  cv_update_save_name("default");

  if(!PrnName) PrnStream=stdout;
  else
  {
    if(!(PrnStream=fopen(PrnName,"wb"))) PrnStream=stdout;
  }

# if 0 //LUDO:
  /* Set up the palette */
  for(J=0;J<16;J++) {
    SetColor(J,Palette[J].R,Palette[J].G,Palette[J].B);
  }
# else
  for (J=0; J < 16; J++) {
    Palette_rgb[J] = psp_sdl_rgb(Palette[J].R,Palette[J].G,Palette[J].B);
  }
# endif

  cv_default_settings();
  psp_joy_default_settings();
  psp_kbd_default_settings();

  cv_update_save_name("");
  cv_load_settings();
  cv_kbd_load();
  cv_joy_load();
  cv_load_cheat();

  myPowerSetClockFrequency(CV.psp_cpu_clock);

  emulator_reset();
  psp_sdl_black_screen();

  J=RunZ80();

  return(1);
}

/** TrashColeco() ********************************************/
/** Free memory allocated by StartColeco().                 **/
/*************************************************************/
void TrashColeco(void)
{
  if(RAM)  free(RAM);
  if(VRAM) free(VRAM);
}

/** WrZ80() **************************************************/
/** Z80 emulation calls this function to write byte V to    **/
/** address A of Z80 address space.                         **/
/*************************************************************/
void WrZ80(register word A,register byte V)
{
  if((A>0x5FFF)&&(A<0x8000))
  {
    A&=0x03FF;
    RAM[0x6000+A]=RAM[0x6400+A]=RAM[0x6800+A]=RAM[0x6C00+A]=
    RAM[0x7000+A]=RAM[0x7400+A]=RAM[0x7800+A]=RAM[0x7C00+A]=V;
  }
}

/** RdZ80() **************************************************/
/** Z80 emulation calls this function to read a byte from   **/
/** address A of Z80 address space. Now  moved to z80.c and **/
/** made inlined to speed things up.                        **/
/*************************************************************/
#ifndef COLEM
byte RdZ80(register word A) { return(RAM[A]); }
#endif

/** PatchZ80() ***********************************************/
/** Z80 emulation calls this function when it encounters a  **/
/** special patch command (ED FE) provided for user needs.  **/
/*************************************************************/
void PatchZ80() {}

/** InZ80() **************************************************/
/** Z80 emulation calls this function to read a byte from   **/
/** a given I/O port.                                       **/
/*************************************************************/
byte InZ80(register word Port)
{
  static byte KeyCodes[16] =
  {
    0x0A,0x0D,0x07,0x0C,0x02,0x03,0x0E,0x05,
    0x01,0x0B,0x06,0x09,0x08,0x04,0x0F,0x0F,
  };

  switch(Port&0xE0)
  {

case 0x40: /* Printer Status */
  if(Adam&&(Port==0x40)) return(0xFF);
  break;

case 0xE0: /* Joysticks Data */
  Port=(Port>>1)&0x01;
  Port=JoyMode?
    (JoyState[Port]>>8):
    (JoyState[Port]&0xF0)|KeyCodes[JoyState[Port]&0x0F];
  return((Port|0xB0)&0x7F);

case 0xA0: /* VDP Status/Data */
  if(Port&0x01) { Port=VDPStatus;VDPStatus&=0x5F;VKey=1; }
  else { Port=VRAM[RVAddr.W];RVAddr.W=(RVAddr.W+1)&0x3FFF; }
  return(Port);

  }

  /* No such port */
  return(NORAM);
}

/** OutZ80() *************************************************/
/** Z80 emulation calls this function to write a byte to a  **/
/** given I/O port.                                         **/
/*************************************************************/
void OutZ80(register word Port,register byte Value)
{
  static byte VR; /* Sound and VDP register storage */

  switch(Port&0xE0)
  {

case 0x80: JoyMode=0;return;
case 0xC0: JoyMode=1;return;
case 0xE0: Write76489(&PSG,Value);return; 

case 0x40:
  if(Adam&&(Port==0x40)) fputc(Value,PrnStream);
  return;

case 0xA0:
  if(Port&0x01)
    if(VKey) { VR=Value;VKey--; }
    else
    {
      VKey++;
      switch(Value&0xC0)
      {
        case 0x80: VDPOut(Value&0x07,VR);break;
        case 0x40: WVAddr.B.l=VR;WVAddr.B.h=Value&0x3F;break;
        case 0x00: RVAddr.B.l=VR;RVAddr.B.h=Value;
      }
    }
  else
    if(VKey)
    { VRAM[WVAddr.W]=Value;WVAddr.W=(WVAddr.W+1)&0x3FFF; }
  return;

  }
}

/** LoopZ80() ************************************************/
/** Z80 emulation calls this function periodically to check **/
/** if the system hardware requires any interrupts.         **/
/*************************************************************/
word LoopZ80()
{
# if 0 //LUDO:
  static byte UCount=0;
# endif

  /* Next scanline */
  CurLine=(CurLine+1)%193;

  /* Refresh scanline if needed */
  if(CurLine<192)
  {
# if 0 //LUDO:
    if(!UCount) 
# endif
      (SCR[ScrMode].Refresh)(CurLine);
    CPU.IPeriod=HPeriod;
    return(INT_NONE);
  }

  /* End of screen reached... */

  /* Set IPeriod to the beginning of next screen */
  CPU.IPeriod=VPeriod-HPeriod*192;

  /* Check joysticks */
  Joysticks();

  /* Flush any accumulated sound changes */
  Sync76489(&PSG,PSG_FLUSH);
    
  /* Refresh screen if needed */
# if 0 //LUDO:
  if(UCount) UCount--;
  else {
  	UCount=UPeriod-1;
  	RefreshScreen();
  }
# else
  RefreshScreen();
# endif

  /* Setting VDPStatus flags */
  VDPStatus=(VDPStatus&0xDF)|0x80;

  /* Checking sprites: */
  if(ScrMode) CheckSprites();

  /* If exit requested, return INT_QUIT */
  if(ExitNow) return(INT_QUIT);

  /* Generate VDP interrupt */
  return(VKey&&(VDP[1]&0x20)? INT_NMI:INT_NONE);
}

/** VDPOut() *************************************************/
/** Emulator calls this function to write byte V into a VDP **/
/** register R.                                             **/
/*************************************************************/
void VDPOut(register byte R,register byte V)
{ 
  register byte J;

  switch(R)  
  {
    case  0: switch(((V&0x0E)>>1)|(VDP[1]&0x18))
             {
               case 0x10: J=0;break;
               case 0x00: J=1;break;
               case 0x01: J=2;break;
               case 0x08: J=3;break;
               default:   J=ScrMode;
             }   
             if(J!=ScrMode)
             {
               ChrTab=VRAM+((long)(VDP[2]&SCR[J].R2)<<10);
               ChrGen=VRAM+((long)(VDP[4]&SCR[J].R4)<<11);
               ColTab=VRAM+((long)(VDP[3]&SCR[J].R3)<<6);
               SprTab=VRAM+((long)(VDP[5]&SCR[J].R5)<<7);
               SprGen=VRAM+((long)VDP[6]<<11);
               ScrMode=J;
             }
             break;
    case  1: switch(((VDP[0]&0x0E)>>1)|(V&0x18))
             {
               case 0x10: J=0;break;
               case 0x00: J=1;break;
               case 0x01: J=2;break;
               case 0x08: J=3;break;
               default:   J=ScrMode;
             }   
             if(J!=ScrMode)
             {
               ChrTab=VRAM+((long)(VDP[2]&SCR[J].R2)<<10);
               ChrGen=VRAM+((long)(VDP[4]&SCR[J].R4)<<11);
               ColTab=VRAM+((long)(VDP[3]&SCR[J].R3)<<6);
               SprTab=VRAM+((long)(VDP[5]&SCR[J].R5)<<7);
               SprGen=VRAM+((long)VDP[6]<<11);
               ScrMode=J;
             }
             break;       
    case  2: ChrTab=VRAM+((long)(V&SCR[ScrMode].R2)<<10);break;
    case  3: ColTab=VRAM+((long)(V&SCR[ScrMode].R3)<<6);break;
    case  4: ChrGen=VRAM+((long)(V&SCR[ScrMode].R4)<<11);break;
    case  5: SprTab=VRAM+((long)(V&SCR[ScrMode].R5)<<7);break;
    case  6: V&=0x3F;SprGen=VRAM+((long)V<<11);break;
    case  7: FGColor=V>>4;BGColor=V&0x0F;break;

  }
  VDP[R]=V;return;
} 

/** CheckSprites() *******************************************/
/** This function is periodically called to check for the   **/
/** sprite collisions and 5th sprite, and set appropriate   **/
/** bits in the VDP status register.                        **/
/*************************************************************/
void CheckSprites(void)
{
  register word LS,LD;
  register byte DH,DV,*PS,*PD,*T;
  byte I,J,N,*S,*D;

  VDPStatus=(VDPStatus&0x9F)|0x1F;
  for(N=0,S=SprTab;(N<32)&&(S[0]!=208);N++,S+=4);

  if(Sprites16x16)
  {
    for(J=0,S=SprTab;J<N;J++,S+=4)
      if(S[3]&0x0F)
        for(I=J+1,D=S+4;I<N;I++,D+=4)
          if(D[3]&0x0F)
          {
            DV=S[0]-D[0];
            if((DV<16)||(DV>240))
            {
              DH=S[1]-D[1];
              if((DH<16)||(DH>240))
              {
                PS=SprGen+((long)(S[2]&0xFC)<<3);
                PD=SprGen+((long)(D[2]&0xFC)<<3);
                if(DV<16) PD+=DV; else { DV=256-DV;PS+=DV; }
                if(DH>240) { DH=256-DH;T=PS;PS=PD;PD=T; }
                while(DV<16)
                {
                  LS=((word)*PS<<8)+*(PS+16);
                  LD=((word)*PD<<8)+*(PD+16);
                  if(LD&(LS>>DH)) break;
                  else { DV++;PS++;PD++; }
                }
                if(DV<16) { VDPStatus|=0x20;return; }
              }
            }
          }
  }
  else
  {
    for(J=0,S=SprTab;J<N;J++,S+=4)
      if(S[3]&0x0F)
        for(I=J+1,D=S+4;I<N;I++,D+=4)
          if(D[3]&0x0F)
          {
            DV=S[0]-D[0];
            if((DV<8)||(DV>248))
            {
              DH=S[1]-D[1];
              if((DH<8)||(DH>248))
              {
                PS=SprGen+((long)S[2]<<3);
                PD=SprGen+((long)D[2]<<3);
                if(DV<8) PD+=DV; else { DV=256-DV;PS+=DV; }
                if(DH>248) { DH=256-DH;T=PS;PS=PD;PD=T; }
                while((DV<8)&&!(*PD&(*PS>>DH))) { DV++;PS++;PD++; }
                if(DV<8) { VDPStatus|=0x20;return; }
              }
            }
          }
  }
}

int
cv_load_rom(char *FileName, int zip_format)
{
  char   SaveName[MAX_PATH+1];
  char*  ExtractName;
  char*  scan;
  int    error;
  size_t unzipped_size;

  error = 1;

  if (zip_format) {

    ExtractName = find_possible_filename_in_zip( FileName, "rom.col");
    if (ExtractName) {
      strncpy(SaveName, FileName, MAX_PATH);
      scan = strrchr(SaveName,'.');
      if (scan) *scan = '\0';
      cv_update_save_name(SaveName);
      char* rom_buffer = extract_file_in_memory ( FileName, ExtractName, &unzipped_size);
      if (rom_buffer) {
        error = ! loc_load_rom_buffer( rom_buffer, unzipped_size );
        free(rom_buffer);
      }
    }

  } else {
    strncpy(SaveName,FileName,MAX_PATH);
    scan = strrchr(SaveName,'.');
    if (scan) *scan = '\0';
    cv_update_save_name(SaveName);
    error = ! loc_load_rom(FileName);
  }

  if (! error ) {
    emulator_reset();
    cv_kbd_load();
    cv_joy_load();
    cv_load_cheat();
    cv_load_settings();
  }

  return error;
}

static int
loc_load_state_sta(char *filename)
{
  colem_save_t sd;
  FILE        *fd;
  int          index;

  emulator_reset();

  memset(&sd,0, sizeof(sd));

  if ((fd = fopen(filename, "rb")) != NULL) {
    if (fread(&sd, sizeof(sd), 1, fd) != 1) {
      fclose(fd);
      return 1;
    }
    if (fread(RAM,0x10000, 1, fd) != 1) {
      fclose(fd);
      return 1;
    }
    if (fread(VRAM, 0x4000, 1, fd) != 1) {
      fclose(fd);
      return 1;
    }
    fclose(fd);
  } else {
    return 1;
  }

  CPU =         sd.CPU;
  PSG =         sd.PSG;
  JoyMode =     sd.JoyMode;
  JoyState[0] = sd.JoyState[0];
  JoyState[1] = sd.JoyState[1];
  ChrGen = VRAM + sd.ChrGen_idx;;
  ChrTab = VRAM + sd.ChrTab_idx;;
  ColTab = VRAM + sd.ColTab_idx;
  SprGen = VRAM + sd.SprGen_idx;
  SprTab = VRAM + sd.SprTab_idx;
  WVAddr = sd.WVAddr;
  RVAddr = sd.RVAddr;
  VKey   =  sd.VKey;
  FGColor = sd.FGColor;
  BGColor = sd.BGColor;
  ScrMode = sd.ScrMode;
  CurLine = sd.CurLine;
  VDPStatus = sd.VDPStatus;

  for (index = 0; index < 8; index++) {
    VDP[index] = sd.VDP[index];
  }
  return 0;
}

static int
loc_load_state_stz(char *filename)
{
  colem_save_t sd;
  gzFile       fd;
  int          index;

  emulator_reset();

  memset(&sd,0, sizeof(sd));

  if ((fd = gzopen(filename, "rb")) != NULL) {
    if (! gzread(fd, &sd, sizeof(sd))) {
      gzclose(fd);
      return 1;
    }
    if (! gzread(fd,RAM,0x10000)) {
      gzclose(fd);
      return 1;
    }
    if (! gzread(fd, VRAM, 0x4000)) {
      gzclose(fd);
      return 1;
    }
    gzclose(fd);
  } else {
    return 1;
  }

  CPU =         sd.CPU;
  PSG =         sd.PSG;
  JoyMode =     sd.JoyMode;
  JoyState[0] = sd.JoyState[0];
  JoyState[1] = sd.JoyState[1];
  ChrGen = VRAM + sd.ChrGen_idx;;
  ChrTab = VRAM + sd.ChrTab_idx;;
  ColTab = VRAM + sd.ColTab_idx;
  SprGen = VRAM + sd.SprGen_idx;
  SprTab = VRAM + sd.SprTab_idx;
  WVAddr = sd.WVAddr;
  RVAddr = sd.RVAddr;
  VKey   =  sd.VKey;
  FGColor = sd.FGColor;
  BGColor = sd.BGColor;
  ScrMode = sd.ScrMode;
  CurLine = sd.CurLine;
  VDPStatus = sd.VDPStatus;

  for (index = 0; index < 8; index++) {
    VDP[index] = sd.VDP[index];
  }
  return 0;
}

static int 
loc_load_state(char *FileName)
{
  char *pszExt;
  if((pszExt = strrchr(FileName, '.'))) {
    if (!strcasecmp(pszExt, ".stz")) {
      return loc_load_state_stz(FileName);
    }
  }
  return loc_load_state_sta(FileName);
}


int
cv_load_state(char *FileName)
{
  char *scan;
  char  SaveName[MAX_PATH+1];
  int   error;

  error = 1;

  strncpy(SaveName,FileName,MAX_PATH);
  scan = strrchr(SaveName,'.');
  if (scan) *scan = '\0';
  cv_update_save_name(SaveName);
  error = loc_load_state(FileName);

  if (! error ) {
    cv_kbd_load();
    cv_joy_load();
    cv_load_cheat();
    cv_load_settings();
  }

  return error;
}

static int
cv_save_state_sta(char *filename)
{
  colem_save_t sd;
  FILE        *fd;
  int          index;

  sd.CPU         = CPU;
  sd.PSG         = PSG;
  sd.JoyMode     = JoyMode;
  sd.JoyState[0] = JoyState[0];
  sd.JoyState[1] = JoyState[1];

  sd.ChrGen_idx  = (long)(ChrGen - VRAM);
  sd.ChrTab_idx  = (long)(ChrTab - VRAM);
  sd.ColTab_idx  = (long)(ColTab - VRAM);
  sd.SprGen_idx  = (long)(SprGen - VRAM);
  sd.SprTab_idx  = (long)(SprTab - VRAM);
  sd.WVAddr      = WVAddr;
  sd.RVAddr      = RVAddr;
  sd.VKey        = VKey;
  sd.FGColor     = FGColor;
  sd.BGColor     = BGColor;
  sd.ScrMode     = ScrMode;
  sd.CurLine     = CurLine;
  sd.VDPStatus   = VDPStatus;

  for (index = 0; index < 8; index++) {
    sd.VDP[index] = VDP[index];
  }

  if ((fd = fopen(filename, "wb")) != NULL) {
    if (fwrite(&sd, sizeof(sd), 1, fd) != 1) {
      fclose(fd);
      return 1;
    }
    if (fwrite(RAM,0x10000, 1, fd) != 1) {
      fclose(fd);
      return 1;
    }
    if (fwrite(VRAM, 0x4000, 1, fd) != 1) {
      fclose(fd);
      return 1;
    }
    fclose(fd);

  } else {
    return 1;
  }
  return 0;
}

static int
cv_save_state_stz(char *filename)
{
  colem_save_t sd;
  gzFile       fd;
  int          index;

  sd.CPU         = CPU;
  sd.PSG         = PSG;
  sd.JoyMode     = JoyMode;
  sd.JoyState[0] = JoyState[0];
  sd.JoyState[1] = JoyState[1];

  sd.ChrGen_idx  = (long)(ChrGen - VRAM);
  sd.ChrTab_idx  = (long)(ChrTab - VRAM);
  sd.ColTab_idx  = (long)(ColTab - VRAM);
  sd.SprGen_idx  = (long)(SprGen - VRAM);
  sd.SprTab_idx  = (long)(SprTab - VRAM);
  sd.WVAddr      = WVAddr;
  sd.RVAddr      = RVAddr;
  sd.VKey        = VKey;
  sd.FGColor     = FGColor;
  sd.BGColor     = BGColor;
  sd.ScrMode     = ScrMode;
  sd.CurLine     = CurLine;
  sd.VDPStatus   = VDPStatus;

  for (index = 0; index < 8; index++) {
    sd.VDP[index] = VDP[index];
  }

  if ((fd = gzopen(filename, "wb")) != NULL) {
    if (! gzwrite(fd, &sd, sizeof(sd))) {
      gzclose(fd);
      return 1;
    }
    if (! gzwrite(fd, RAM,0x10000)) {
      gzclose(fd);
      return 1;
    }
    if (! gzwrite(fd, VRAM, 0x4000)) {
      gzclose(fd);
      return 1;
    }
    gzclose(fd);

  } else {
    return 1;
  }
  return 0;
}

static int 
cv_save_state(char *FileName)
{
  char *pszExt;
  if((pszExt = strrchr(FileName, '.'))) {
    if (!strcasecmp(pszExt, ".stz")) {
      return cv_save_state_stz(FileName);
    }
  }
  return cv_save_state_sta(FileName);
}

int
cv_snapshot_save_slot(int save_id)
{
  char        FileName[MAX_PATH+1];
  struct stat aStat;
  int         error;

  error = 1;

  if (save_id < CV_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.stz", CV.cv_home_dir, CV.cv_save_name, save_id);
    error = cv_save_state(FileName);
    if (! error) {
      if (! stat(FileName, &aStat)) {
        CV.cv_save_state[save_id].used  = 1;
        CV.cv_save_state[save_id].thumb = 0;
        CV.cv_save_state[save_id].date  = aStat.st_mtime;
        snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.png", CV.cv_home_dir, CV.cv_save_name, save_id);
        if (psp_sdl_save_thumb_png(CV.cv_save_state[save_id].surface, FileName)) {
          CV.cv_save_state[save_id].thumb = 1;
        }
      }
    }
  }
  return error;
}

int
cv_snapshot_load_slot(int load_id)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  if (load_id < CV_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.stz", CV.cv_home_dir, CV.cv_save_name, load_id);
    error = loc_load_state(FileName);
  }
  return error;
}

int
cv_snapshot_del_slot(int save_id)
{
  char  FileName[MAX_PATH+1];
  struct stat aStat;
  int   error;

  error = 1;

  if (save_id < CV_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.stz", CV.cv_home_dir, CV.cv_save_name, save_id);
    error = remove(FileName);
    if (! error) {
      CV.cv_save_state[save_id].used  = 0;
      CV.cv_save_state[save_id].thumb = 0;
      memset(&CV.cv_save_state[save_id].date, 0, sizeof(time_t));

      /* We keep always thumbnail with id 0, to have something to display in the file requester */ 
      if (save_id != 0) {
        snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.png", CV.cv_home_dir, CV.cv_save_name, save_id);
        if (! stat(FileName, &aStat)) {
          remove(FileName);
        }
      }
    }
  }

  return error;
}

static int
loc_cv_save_cheat(char *chFileName)
{
  FILE* FileDesc;
  int   cheat_num;
  int   error = 0;

  FileDesc = fopen(chFileName, "w");
  if (FileDesc != (FILE *)0 ) {

    for (cheat_num = 0; cheat_num < CV_MAX_CHEAT; cheat_num++) {
      CV_cheat_t* a_cheat = &CV.cv_cheat[cheat_num];
      if (a_cheat->type != CV_CHEAT_NONE) {
        fprintf(FileDesc, "%d,%x,%x,%s\n", 
                a_cheat->type, a_cheat->addr, a_cheat->value, a_cheat->comment);
      }
    }
    fclose(FileDesc);

  } else {
    error = 1;
  }

  return error;
}

int
cv_save_cheat(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/cht/%s.cht", CV.cv_home_dir, CV.cv_save_name);
  error = loc_cv_save_cheat(FileName);

  return error;
}

static int
loc_cv_load_cheat(char *chFileName)
{
  char  Buffer[512];
  char *Scan;
  char *Field;
  unsigned int  cheat_addr;
  unsigned int  cheat_value;
  unsigned int  cheat_type;
  char         *cheat_comment;
  int           cheat_num;
  FILE* FileDesc;

  memset(CV.cv_cheat, 0, sizeof(CV.cv_cheat));
  cheat_num = 0;

  FileDesc = fopen(chFileName, "r");
  if (FileDesc == (FILE *)0 ) return 0;

  while (fgets(Buffer,512, FileDesc) != (char *)0) {

    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    /* %d, %x, %x, %s */
    Field = Buffer;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%d", &cheat_type) != 1) continue;
    Field = Scan + 1;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%x", &cheat_addr) != 1) continue;
    Field = Scan + 1;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%x", &cheat_value) != 1) continue;
    Field = Scan + 1;
    cheat_comment = Field;

    if (cheat_type <= CV_CHEAT_NONE) continue;

    CV_cheat_t* a_cheat = &CV.cv_cheat[cheat_num];

    a_cheat->type  = cheat_type;
    a_cheat->addr  = cheat_addr & (CV_CHEAT_RAM_SIZE-1);
    a_cheat->value = cheat_value;
    strncpy(a_cheat->comment, cheat_comment, sizeof(a_cheat->comment));
    a_cheat->comment[sizeof(a_cheat->comment)-1] = 0;

    if (++cheat_num >= CV_MAX_CHEAT) break;
  }
  fclose(FileDesc);

  return 0;
}

int
cv_load_cheat()
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/cht/%s.cht", CV.cv_home_dir, CV.cv_save_name);
  error = loc_cv_load_cheat(FileName);

  return error;
}

int
cv_load_file_cheat(char *FileName)
{
  return loc_cv_load_cheat(FileName);
}

void
cv_apply_cheats()
{
  int cheat_num;
  for (cheat_num = 0; cheat_num < CV_MAX_CHEAT; cheat_num++) {
    CV_cheat_t* a_cheat = &CV.cv_cheat[cheat_num];
    if (a_cheat->type == CV_CHEAT_ENABLE) {
      RAM[a_cheat->addr] = a_cheat->value;
    }
  }
}


void
cv_treat_command_key(int cv_idx)
{
  int new_render;

  switch (cv_idx) 
  {
    case CVC_FPS: CV.cv_view_fps = ! CV.cv_view_fps;
    break;
    case CVC_JOY: CV.psp_reverse_analog = ! CV.psp_reverse_analog;
    break;
    case CVC_RENDER: 
      psp_sdl_black_screen();
      new_render = CV.cv_render_mode + 1;
      if (new_render > CV_LAST_RENDER) new_render = 0;
      CV.cv_render_mode = new_render;
    break;
    case CVC_LOAD: psp_main_menu_load_current();
    break;
    case CVC_SAVE: psp_main_menu_save_current(); 
    break;
    case CVC_RESET: 
       psp_sdl_black_screen();
       emulator_reset(); 
    break;
    case CVC_AUTOFIRE: 
       kbd_change_auto_fire(! CV.cv_auto_fire);
    break;
    case CVC_DECFIRE: 
      if (CV.cv_auto_fire_period > 0) CV.cv_auto_fire_period--;
    break;
    case CVC_INCFIRE: 
      if (CV.cv_auto_fire_period < 19) CV.cv_auto_fire_period++;
    break;
    case CVC_SCREEN: psp_screenshot_mode = 10;
    break;
  }
}
