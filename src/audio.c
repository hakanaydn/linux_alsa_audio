#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include "stdio.h"
#include "stdint.h"
#include <string.h>
#include <stdlib.h>
#include "../include/audio.h"
#include <unistd.h>

#define PERIOD_SIZE 1024
#define BUF_SIZE (PERIOD_SIZE * 2)
#define PCM_DEVICE "default"

#define SLOT 25


// Declaration of thread condition variable 
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER; 
// declaring mutex 
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 

pthread_t threads_audio_read;
pthread_t threads_audio_write;

snd_pcm_t *playback_handle;
snd_pcm_t *capture_handle;

uint8_t  od_buf[SLOT][BUF_SIZE];
uint8_t *pOd_buf[SLOT];
uint8_t  od_flag[SLOT];
uint8_t  od_write= 0;
uint8_t  od_read = 0;

void buf_init()
{
  for(uint8_t i = 0; i < SLOT; i++)
  {
    pOd_buf[i] = &od_buf[i][0];
    od_flag[i] = 0;
    memset(pOd_buf[i],0,BUF_SIZE);
  }
  od_read = 0;
  od_write = 0;
}


void print_pcm_state(snd_pcm_t *handle, char *name) {
  switch (snd_pcm_state(handle)) {
  case SND_PCM_STATE_OPEN:
    printf("state open %s\n", name);
    break;

  case SND_PCM_STATE_SETUP:
    printf("state setup %s\n", name);
    break;

  case SND_PCM_STATE_PREPARED:
    printf("state prepare %s\n", name);
    break;

  case SND_PCM_STATE_RUNNING:
    printf("state running %s\n", name);
    break;

  case SND_PCM_STATE_XRUN:
    printf("state xrun %s\n", name);
    break;

  default:
    printf("state other %s\n", name);
    break;

  }
}


int setparams(snd_pcm_t *handle, char *name) {
  snd_pcm_hw_params_t *hw_params;
  int err;


  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
    fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
       snd_strerror (err));
    exit (1);
  }
         
  if ((err = snd_pcm_hw_params_any (handle, hw_params)) < 0) {
    fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
       snd_strerror (err));
    exit (1);
  }
  
  if ((err = snd_pcm_hw_params_set_access (handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf (stderr, "cannot set access type (%s)\n",
       snd_strerror (err));
    exit (1);
  }
  
  if ((err = snd_pcm_hw_params_set_format (handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf (stderr, "cannot set sample format (%s)\n",
       snd_strerror (err));
    exit (1);
  }

  unsigned int rate = 8000;
  if ((err = snd_pcm_hw_params_set_rate_near (handle, hw_params, &rate, 0)) < 0) {
    fprintf (stderr, "cannot set sample rate (%s)\n",
       snd_strerror (err));
    exit (1);
  }
  printf("Rate for %s is %d\n", name, rate);
  
  if ((err = snd_pcm_hw_params_set_channels (handle, hw_params, 1)) < 0) {
    fprintf (stderr, "cannot set channel count (%s)\n",
       snd_strerror (err));
    exit (1);
  }
  
  snd_pcm_uframes_t buffersize = BUF_SIZE;
  if ((err = snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffersize)) < 0) {
    printf("Unable to set buffer size %li: %s\n", BUF_SIZE, snd_strerror(err));
    exit (1);;
  }

  snd_pcm_uframes_t periodsize = PERIOD_SIZE;
  fprintf(stderr, "period size now %d\n", periodsize);
  if ((err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, &periodsize, 0)) < 0) {
    printf("Unable to set period size %li: %s\n", periodsize, snd_strerror(err));
    exit (1);
  }

  if ((err = snd_pcm_hw_params (handle, hw_params)) < 0) {
    fprintf (stderr, "cannot set parameters (%s)\n",
       snd_strerror (err));
    exit (1);
  }

  snd_pcm_uframes_t p_psize;
  snd_pcm_hw_params_get_period_size(hw_params, &p_psize, NULL);
  fprintf(stderr, "period size %d\n", p_psize);

  snd_pcm_hw_params_get_buffer_size(hw_params, &p_psize);
  fprintf(stderr, "buffer size %d\n", p_psize);

  snd_pcm_hw_params_free (hw_params);
  
  if ((err = snd_pcm_prepare (handle)) < 0) {
    fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
       snd_strerror (err));
    exit (1);
  }

  return 0;
}

int set_sw_params(snd_pcm_t *handle, char *name) {
  snd_pcm_sw_params_t *swparams;
  int err;

  snd_pcm_sw_params_alloca(&swparams);

  err = snd_pcm_sw_params_current(handle, swparams);
  if (err < 0) {
    fprintf(stderr, "Broken configuration for this PCM: no configurations available\n");
    exit(1);
  }
  
  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, PERIOD_SIZE);
  if (err < 0) {
    printf("Unable to set start threshold: %s\n", snd_strerror(err));
    return err;
  }
  err = snd_pcm_sw_params_set_avail_min(handle, swparams, PERIOD_SIZE);
  if (err < 0) {
    printf("Unable to set avail min: %s\n", snd_strerror(err));
    return err;
  }

  if (snd_pcm_sw_params(handle, swparams) < 0) {
    fprintf(stderr, "unable to install sw params:\n");
    exit(1);
  }

  return 0;
}

void *audio_writeAlsa_thread(void *threadid) 
{
  int err;

  while(1)
  {
    if(od_flag[od_read] == 1)
    {
      if ((err = snd_pcm_writei (playback_handle,&pOd_buf[od_read], BUF_SIZE)) != BUF_SIZE) 
      {
        if (err < 0) 
        {
          fprintf (stderr, "write to audio interface failed (%s)\n",
          snd_strerror (err));
        } 
        else 
        {
          fprintf (stderr, "write to audio interface failed after %d frames\n", err);
        }
        snd_pcm_prepare(playback_handle);
      }
      else
      {
        od_flag[od_read] = 0;
        od_read = (od_read + 1) % SLOT;
      }
    }
    pthread_cond_signal(&cond1); 
  }
  pthread_exit(NULL);
}

void *audio_readAlsa_thread(void *threadid) 
{
  int nread;

  while(1)
  {
    if(od_flag[od_write] == 1)
    {
      printf("ERR\n");
    }

    // acquire a lock 
    pthread_mutex_lock(&lock); 

    if ((nread = snd_pcm_readi (capture_handle, &pOd_buf[od_write], BUF_SIZE)) != BUF_SIZE) 
    {
      if (nread < 0) 
      {
        fprintf (stderr, "read from audio interface failed (%s)\n",
        snd_strerror (nread));
      } 
      else 
      {
        fprintf (stderr, "read from audio interface failed after %d frames\n", nread);
      } 
      snd_pcm_prepare(capture_handle);
    }
    else
    { 
      od_flag[od_write] = 1;
      od_write = (od_write + 1) % SLOT;
      pthread_cond_wait(&cond1, &lock);
    }
    // release lock 
    pthread_mutex_unlock(&lock); 
  }
  pthread_exit(NULL);
}


void init_alsa (void)
{
  int i;
  int err;
  int buf[BUF_SIZE];
  snd_pcm_hw_params_t *hw_params;
  FILE *fin;
  size_t nread;
  snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;

  buf_init();

  /**** Out card *******/
  if ((err = snd_pcm_open (&playback_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf (stderr, "cannot open audio device (%s)\n", 
    snd_strerror (err));
    exit (1);
  }

  setparams(playback_handle, "playback");
  set_sw_params(playback_handle, "playback");

  /*********** In card **********/
  if ((err = snd_pcm_open (&capture_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
    fprintf (stderr, "cannot open audio device (%s)\n", 
    snd_strerror (err));
    exit (1);
  }

  setparams(capture_handle, "capture");
  set_sw_params(capture_handle, "capture");

  if ((err = snd_pcm_prepare (playback_handle)) < 0) {
    fprintf (stderr, "cannot prepare playback audio interface for use (%s)\n",
    snd_strerror (err));
    exit (1);
  }

  /**************** stuff something into the playback buffer ****************/
  if (snd_pcm_format_set_silence(format, buf, 2*BUF_SIZE) < 0) {
    fprintf(stderr, "silence error\n");
    exit(1);
  }

  int n = 0;
  while (n++ < 2) {
    if (snd_pcm_writei (playback_handle, buf, BUF_SIZE) < 0) {
      fprintf(stderr, "write error\n");
      exit(1);
    }
  }

  int rc2 = pthread_create(&threads_audio_write, NULL, audio_writeAlsa_thread, NULL);
  int rc1 = pthread_create(&threads_audio_read, NULL, audio_readAlsa_thread,NULL);

  //pthread_join(threads_audio_read, NULL); 
  //pthread_join(threads_audio_write, NULL); 
}

void alsa_close(void)
{
  snd_pcm_drain(playback_handle); 
  snd_pcm_close (playback_handle);
}

