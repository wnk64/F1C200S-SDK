#ifndef TEST_FAKE_ALSA_H
#define TEST_FAKE_ALSA_H
typedef struct snd_pcm snd_pcm_t;
typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;
#define SND_PCM_STREAM_PLAYBACK 0
#define SND_PCM_NONBLOCK 1
#define SND_PCM_FORMAT_S16_BE 1
#define SND_PCM_ACCESS_RW_INTERLEAVED 1
int snd_config_update_free_global(void);
int snd_pcm_open(snd_pcm_t **, const char *, int, int);
int snd_pcm_set_params(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
int snd_pcm_drop(snd_pcm_t *);
int snd_pcm_close(snd_pcm_t *);
snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t *, const void *, snd_pcm_uframes_t);
int snd_pcm_wait(snd_pcm_t *, int);
int snd_pcm_recover(snd_pcm_t *, int, int);
const char *snd_strerror(int);
#endif
