/*
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
#include <string.h>
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_image.h>

#include "Coleco.h"
#include "psp_sdl.h"
#include "psp_sound.h"

SDL_AudioSpec wanted;

#define NOISEBSIZE 0x8000
static short noise[NOISEBSIZE];

typedef struct
{
  unsigned int spos;
  unsigned int sinc;
  unsigned int vol;
} Channel;

static short square[]={
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
};

static int NoiseGen=1;
static Channel chan[4] = {
  {0,0,0},
  {0,0,0},
  {0,0,0} 
};

static short* sdl_sound_buffer = NULL;

static void 
snd_Mixer(void *udata, short *stream, int len)
{
  int i;
  short v0,v1,v2,v3;
  long s; 

  if (sdl_sound_buffer) {

# if defined(DINGUX_MODE)
    short* scan_buffer = stream;
# else
    short* scan_buffer = sdl_sound_buffer;
# endif
    if (CV.cv_snd_enable) {
      v0=chan[0].vol;
      v1=chan[1].vol;
      v2=chan[2].vol;
      v3=chan[3].vol;

      for (i=0;i<len>>2;i++) {
        s =((v0*square[(chan[0].spos>>8)&0x3f])>>10);
        s+=((v1*square[(chan[1].spos>>8)&0x3f])>>10);
        s+=((v2*square[(chan[2].spos>>8)&0x3f])>>10);
        s+=((v3*noise[(chan[3].spos>>8)&(NOISEBSIZE-1)])>>10);      
        *scan_buffer++ = (short)(s);
        *scan_buffer++ = (short)(s);
        chan[0].spos += chan[0].sinc;
        chan[1].spos += chan[1].sinc;
        chan[2].spos += chan[2].sinc;
        chan[3].spos += chan[3].sinc;  
      }
# if !defined(DINGUX_MODE)
      long volume = (SDL_MIX_MAXVOLUME * gp2xGetSoundVolume()) / 100;
      SDL_MixAudio(stream, (unsigned char *)sdl_sound_buffer, len, volume);
# endif
    } else {
      memset(stream, 0, len);
    }
  }
}

void 
Sound_Init(void)
{
  int i;

  chan[0].vol = 0;
  chan[1].vol = 0;
  chan[2].vol = 0;
  chan[3].vol = 0;
  chan[0].sinc = 0;
  chan[1].sinc = 0;
  chan[2].sinc = 0;
  chan[3].sinc = 0;
  for(i=0;i<NOISEBSIZE;i++)
  {
      NoiseGen<<=1;
      if(NoiseGen&0x80000000) NoiseGen^=0x08000001;
      noise[i]=(NoiseGen&1? 32767:-32767);
  }
}

void 
snd_Sound(int C, int F, int V)
{
  chan[C].vol = V;
  chan[C].sinc = F>>1;
}


/** Play() ***************************************************/
/** Log and play sound of given frequency (Hz) and volume   **/
/** (0..255) via given channel (0..3).                      **/
/*************************************************************/
void Play(int C,int F,int V)
{
  /* Play actual sound */
  snd_Sound(C,F,V);
}


int 
audio_align_samples(int given)
{
  int actual = 1;
  while (actual < given) {
    actual <<= 1;
  }
  return actual; // return the closest match as 2^n
}

int
psp_sdl_init_sound()
{
  wanted.freq     = 44100;
  wanted.format   = AUDIO_S16;
  wanted.channels = 2;
  wanted.samples  = audio_align_samples(wanted.freq / 60 );
  wanted.callback = snd_Mixer;
  wanted.userdata = NULL;

  /* Open the audio device, forcing the desired format */
  if ( SDL_OpenAudio(&wanted, NULL) < 0 ) {
    fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
    return(0);
  }

  sdl_sound_buffer = malloc(2 * sizeof(short) * wanted.samples);
  SDL_Delay(1000);        // Give sound some time to init
  SDL_PauseAudio(0);

  gp2xInitSoundVolume();

  return(1);
}

void 
audio_pause(void)
{
  SDL_PauseAudio(1);
}

void 
audio_resume(void)
{
  if (CV.cv_snd_enable) {
    SDL_PauseAudio(0);
  }
}

void 
audio_shutdown(void)
{
  SDL_CloseAudio();
}
