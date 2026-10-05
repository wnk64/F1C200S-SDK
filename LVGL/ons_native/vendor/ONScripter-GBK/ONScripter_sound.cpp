/* -*- C++ -*-
 * 
 *  ONScripter_sound.cpp - Methods for playing sound
 *
 *  Copyright (c) 2001-2015 Ogapee. All rights reserved.
 *
 *  ogapee@aqua.dti2.ne.jp
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ONScripter.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static void gbkSoundTrace(const char *fmt, ...)
{
    static int count = 0;
    if (!getenv("ONS_GBK_SOUND_TRACE") || count++ > 200) return;
    FILE *fp = fopen("/tmp/ons_gbk_sound_trace.log", "a");
    if (!fp) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
}
#include <new>
#include <string.h>
#include <stdlib.h>
#include <vorbis/vorbisfile.h>
#if defined(LINUX)
#include <signal.h>
#endif

#ifdef ANDROID
extern "C"
{
#include <jni.h>
#include <android/log.h>
static JavaVM *jniVM = NULL;
static jobject JavaONScripter = NULL;
static jmethodID JavaPlayVideo = NULL;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    jniVM = vm;
    return JNI_VERSION_1_2;
};

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    jniVM = vm;
};

#ifndef SDL_JAVA_PACKAGE_PATH
#error You have to define SDL_JAVA_PACKAGE_PATH to your package path with dots replaced with underscores, for example "com_example_SanAngeles"
#endif
#define JAVA_EXPORT_NAME2(name,package) Java_##package##_##name
#define JAVA_EXPORT_NAME1(name,package) JAVA_EXPORT_NAME2(name,package)
#define JAVA_EXPORT_NAME(name) JAVA_EXPORT_NAME1(name,SDL_JAVA_PACKAGE_PATH)

JNIEXPORT jint JNICALL JAVA_EXPORT_NAME(ONScripter_nativeInitJavaCallbacks) (JNIEnv * jniEnv, jobject thiz)
{
    JavaONScripter = jniEnv->NewGlobalRef(thiz);
    jclass JavaONScripterClass = jniEnv->GetObjectClass(JavaONScripter);
    JavaPlayVideo = jniEnv->GetMethodID(JavaONScripterClass, "playVideo", "([C)V");
}
}

void playVideoAndroid(const char *filename)
{
    JNIEnv * jniEnv = NULL;
    jniVM->AttachCurrentThread(&jniEnv, NULL);

    if (!jniEnv){
        __android_log_print(ANDROID_LOG_ERROR, "ONS", "ONScripter::playVideoAndroid: Java VM AttachCurrentThread() failed");
        return;
    }

    jchar *jc = new jchar[strlen(filename)];
    for (int i=0 ; i<strlen(filename) ; i++)
        jc[i] = filename[i];
    jcharArray jca = jniEnv->NewCharArray(strlen(filename));
    jniEnv->SetCharArrayRegion(jca, 0, strlen(filename), jc);
    jniEnv->CallVoidMethod( JavaONScripter, JavaPlayVideo, jca );
    delete[] jc;
}
#endif

#if defined(IOS)
extern "C" void playVideoIOS(const char *filename, bool click_flag, bool loop_flag);
#endif

#if defined(USE_AVIFILE)
#include "AVIWrapper.h"
#endif

#if defined(USE_SMPEG)
#include <smpeg.h>
extern "C" void mp3callback( void *userdata, Uint8 *stream, int len )
{
    SMPEG_playAudio( (SMPEG*)userdata, stream, len );
}
#endif

extern bool ext_music_play_once_flag;

extern "C"{
    extern void musicFinishCallback();
    extern Uint32 SDLCALL cdaudioCallback( Uint32 interval, void *param );
}
extern void midiCallback( int sig );
extern SDL_TimerID timer_cdaudio_id;

extern SDL_TimerID timer_bgmfade_id;
extern "C" Uint32 SDLCALL bgmfadeCallback( Uint32 interval, void *param );


struct GBKOggMemory {
    const unsigned char *data;
    size_t length;
    size_t pos;
};

static size_t gbkOggRead(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    GBKOggMemory *mem = (GBKOggMemory*)datasource;
    size_t bytes = size * nmemb;
    if (bytes > mem->length - mem->pos) bytes = mem->length - mem->pos;
    if (bytes) memcpy(ptr, mem->data + mem->pos, bytes);
    mem->pos += bytes;
    return size ? bytes / size : 0;
}

static int gbkOggSeek(void *datasource, ogg_int64_t offset, int whence)
{
    GBKOggMemory *mem = (GBKOggMemory*)datasource;
    ogg_int64_t next;
    if (whence == SEEK_SET) next = offset;
    else if (whence == SEEK_CUR) next = (ogg_int64_t)mem->pos + offset;
    else if (whence == SEEK_END) next = (ogg_int64_t)mem->length + offset;
    else return -1;
    if (next < 0 || (size_t)next > mem->length) return -1;
    mem->pos = (size_t)next;
    return 0;
}

static long gbkOggTell(void *datasource)
{
    return (long)((GBKOggMemory*)datasource)->pos;
}


static OggVorbis_File gbk_music_vf;
static GBKOggMemory gbk_music_mem;
static bool gbk_music_open = false;
static bool gbk_music_loop = false;

static void gbkCloseOggMusic()
{
    if (gbk_music_open) {
        ov_clear(&gbk_music_vf);
        gbk_music_open = false;
    }
}

static void gbkOggMusicCallback(void *, Uint8 *stream, int len)
{
    int filled = 0;
    int section = 0;
    while (filled < len) {
        long ret = ov_read(&gbk_music_vf, (char*)stream + filled, len - filled, 0, 2, 1, &section);
        if (ret > 0) {
            filled += ret;
            continue;
        }
        if (ret == 0 && gbk_music_loop) {
            ov_raw_seek(&gbk_music_vf, 0);
            continue;
        }
        break;
    }
    if (filled < len) memset(stream + filled, 0, len - filled);
}

static bool gbkOpenOggMusicFromMemory(const unsigned char *buffer, long length, bool loop_flag)
{
    gbkCloseOggMusic();
    gbk_music_mem.data = buffer;
    gbk_music_mem.length = (size_t)length;
    gbk_music_mem.pos = 0;
    gbk_music_loop = loop_flag;

    ov_callbacks cb;
    cb.read_func = gbkOggRead;
    cb.seek_func = gbkOggSeek;
    cb.close_func = NULL;
    cb.tell_func = gbkOggTell;
    if (ov_open_callbacks(&gbk_music_mem, &gbk_music_vf, NULL, 0, cb) < 0) return false;
    gbk_music_open = true;
    Mix_HookMusic(gbkOggMusicCallback, NULL);
    return true;
}

static Mix_Chunk *gbkLoadOggChunk(const unsigned char *buffer, long length)
{
    GBKOggMemory mem = { buffer, (size_t)length, 0 };
    ov_callbacks cb;
    cb.read_func = gbkOggRead;
    cb.seek_func = gbkOggSeek;
    cb.close_func = NULL;
    cb.tell_func = gbkOggTell;

    OggVorbis_File vf;
    if (ov_open_callbacks(&mem, &vf, NULL, 0, cb) < 0) return NULL;

    size_t cap = 65536;
    size_t used = 0;
    unsigned char *pcm = (unsigned char*)malloc(cap);
    if (!pcm) {
        ov_clear(&vf);
        return NULL;
    }

    int section = 0;
    while (1) {
        if (used + 4096 > cap) {
            cap *= 2;
            unsigned char *next = (unsigned char*)realloc(pcm, cap);
            if (!next) {
                free(pcm);
                ov_clear(&vf);
                return NULL;
            }
            pcm = next;
        }
        long ret = ov_read(&vf, (char*)pcm + used, 4096, 0, 2, 1, &section);
        if (ret == 0) break;
        if (ret < 0) continue;
        used += ret;
    }
    ov_clear(&vf);

    if (!used) {
        free(pcm);
        return NULL;
    }

    Mix_Chunk *chunk = Mix_QuickLoad_RAW(pcm, (Uint32)used);
    if (!chunk) {
        free(pcm);
        return NULL;
    }
    chunk->allocated = 1;
    return chunk;
}

#define TMP_MUSIC_FILE "tmp.mus"
#define ONS_GBK_MUSIC_TMP_FILE "/tmp/ons_gbk_music.tmp"

int ONScripter::playSound(const char *filename, int format, bool loop_flag, int channel)
{
    if ( !audio_open_flag ) {
        gbkSoundTrace("GBK_SOUND skip audio_closed file=%s format=0x%x channel=%d", filename, format, channel);
        return SOUND_NONE;
    }

    long length = script_h.cBR->getFileLength( filename );
    gbkSoundTrace("GBK_SOUND request file=%s format=0x%x loop=%d channel=%d len=%ld", filename, format, loop_flag ? 1 : 0, channel, length);
    if (length == 0) return SOUND_NONE;

    unsigned char *buffer;

    if (format & SOUND_MUSIC && 
        length == music_buffer_length &&
        music_buffer ){
        buffer = music_buffer;
    }
    else{
        buffer = new(std::nothrow) unsigned char[length];
        if (buffer == NULL){
            fprintf( stderr, "failed to load [%s] because file size [%lu] is too large.\n", filename, length);
            return SOUND_NONE;
        }
        script_h.cBR->getFile( filename, buffer );
    }
    
    if (format & SOUND_MUSIC){
        if (length >= 4 && buffer[0] == 'O' && buffer[1] == 'g' && buffer[2] == 'g' && buffer[3] == 'S') {
            if (gbkOpenOggMusicFromMemory(buffer, length, (music_play_loop_flag&&music_loopback_offset==0.0))) {
                Mix_VolumeMusic( music_volume );
                gbkSoundTrace("GBK_SOUND music oggstream file=%s len=%ld volume=%d", filename, length, music_volume);
                music_buffer = buffer;
                music_buffer_length = length;
                return SOUND_MUSIC;
            }
            gbkSoundTrace("GBK_SOUND music oggstream fail file=%s len=%ld", filename, length);
        }
        FILE *gbk_music_fp = ::fopen(ONS_GBK_MUSIC_TMP_FILE, "wb");
        if (gbk_music_fp) {
            fwrite(buffer, 1, length, gbk_music_fp);
            fclose(gbk_music_fp);
            music_info = Mix_LoadMUS( ONS_GBK_MUSIC_TMP_FILE );
            gbkSoundTrace("GBK_SOUND music loadfile file=%s tmp=%s ptr=%p err=%s", filename, ONS_GBK_MUSIC_TMP_FILE, music_info, Mix_GetError());
        }
        else {
            music_info = Mix_LoadMUS_RW( SDL_RWFromMem( buffer, length ) );
            gbkSoundTrace("GBK_SOUND music loadrw file=%s ptr=%p err=%s", filename, music_info, Mix_GetError());
        }
        Mix_VolumeMusic( music_volume );
        Mix_HookMusicFinished( musicFinishCallback );
        int gbk_music_ret = Mix_PlayMusic( music_info, (music_play_loop_flag&&music_loopback_offset==0.0)?-1:0 );
        gbkSoundTrace("GBK_SOUND music play file=%s ret=%d volume=%d err=%s", filename, gbk_music_ret, music_volume, Mix_GetError());
        if ( gbk_music_ret == 0 ){
            music_buffer = buffer;
            music_buffer_length = length;
            return SOUND_MUSIC;
        }
    }
    
    if (format & SOUND_CHUNK){
        Mix_Chunk *chunk = Mix_LoadWAV_RW(SDL_RWFromMem(buffer, length), 1);
        if (!chunk && length <= 524288 && length >= 4 && buffer[0] == 'O' && buffer[1] == 'g' && buffer[2] == 'g' && buffer[3] == 'S') {
            chunk = gbkLoadOggChunk(buffer, length);
            gbkSoundTrace("GBK_SOUND chunk oggdecode file=%s ptr=%p err=%s", filename, chunk, Mix_GetError());
        }
        else {
            gbkSoundTrace("GBK_SOUND chunk load file=%s ptr=%p err=%s", filename, chunk, Mix_GetError());
        }
        if (playWave(chunk, format, loop_flag, channel) == 0){
            delete[] buffer;
            return SOUND_CHUNK;
        }
    }

    /* check WMA */
    if ( buffer[0] == 0x30 && buffer[1] == 0x26 &&
         buffer[2] == 0xb2 && buffer[3] == 0x75 ){
        delete[] buffer;
        return SOUND_OTHER;
    }

    if (format & SOUND_MIDI){
        FILE *fp;
        if ( (fp = fopen(TMP_MUSIC_FILE, "wb", true)) == NULL){
            fprintf(stderr, "can't open temporaly MIDI file %s\n", TMP_MUSIC_FILE);
        }
        else{
            fwrite(buffer, 1, length, fp);
            fclose( fp );
            ext_music_play_once_flag = !loop_flag;
            if (playMIDI(loop_flag) == 0){
                delete[] buffer;
                return SOUND_MIDI;
            }
        }
    }

    delete[] buffer;
    
    return SOUND_OTHER;
}

void ONScripter::playCDAudio()
{
    if ( cdaudio_flag ){
#ifdef USE_CDROM
        if ( cdrom_info ){
            int length = cdrom_info->track[current_cd_track - 1].length / 75;
            SDL_CDPlayTracks( cdrom_info, current_cd_track - 1, 0, 1, 0 );
            timer_cdaudio_id = SDL_AddTimer( length * 1000, cdaudioCallback, NULL );
        }
#endif
    }
    else{
        char filename[256];
        sprintf( filename, "cd\\track%2.2d.mp3", current_cd_track );
        int ret = playSound( filename, SOUND_MUSIC, cd_play_loop_flag );
        if (ret == SOUND_MUSIC) return;

        sprintf( filename, "cd\\track%2.2d.ogg", current_cd_track );
        ret = playSound( filename, SOUND_MUSIC, cd_play_loop_flag );
        if (ret == SOUND_MUSIC) return;

        sprintf( filename, "cd\\track%2.2d.wav", current_cd_track );
        ret = playSound( filename, SOUND_MUSIC|SOUND_CHUNK, cd_play_loop_flag, MIX_BGM_CHANNEL );
    }
}

int ONScripter::playWave(Mix_Chunk *chunk, int format, bool loop_flag, int channel)
{
    if (!chunk) {
        gbkSoundTrace("GBK_SOUND wave skip null format=0x%x channel=%d err=%s", format, channel, Mix_GetError());
        return -1;
    }

    Mix_Pause( channel );
    if ( wave_sample[channel] ) Mix_FreeChunk( wave_sample[channel] );
    wave_sample[channel] = chunk;

    if      (channel == 0)               Mix_Volume( channel, voice_volume * MIX_MAX_VOLUME / 100 );
    else if (channel == MIX_BGM_CHANNEL) Mix_Volume( channel, music_volume * MIX_MAX_VOLUME / 100 );
    else                                 Mix_Volume( channel, se_volume * MIX_MAX_VOLUME / 100 );

    if ( !(format & SOUND_PRELOAD) ) {
        int gbk_wave_ret = Mix_PlayChannel( channel, wave_sample[channel], loop_flag?-1:0 );
        gbkSoundTrace("GBK_SOUND wave play ret=%d format=0x%x channel=%d loop=%d vol=%d err=%s", gbk_wave_ret, format, channel, loop_flag ? 1 : 0, Mix_Volume(channel, -1), Mix_GetError());
    }

    return 0;
}

int ONScripter::playMIDI(bool loop_flag)
{
    Mix_SetMusicCMD(midi_cmd);
    
    char midi_filename[256];
    sprintf(midi_filename, "%s%s", save_dir?save_dir:archive_path, TMP_MUSIC_FILE);
    if ((midi_info = Mix_LoadMUS(midi_filename)) == NULL) return -1;

    int midi_looping = loop_flag ? -1 : 0;

#if defined(LINUX)
    signal(SIGCHLD, midiCallback);
    if (midi_cmd) midi_looping = 0;
#endif
    
    Mix_VolumeMusic(music_volume);
    Mix_PlayMusic(midi_info, midi_looping);
    current_cd_track = -2; 
    
    return 0;
}

int ONScripter::playMPEG(const char *filename, bool click_flag, bool loop_flag)
{
    unsigned long length = script_h.cBR->getFileLength( filename );
    if (length == 0){
        fprintf( stderr, " *** can't find file [%s] ***\n", filename );
        return 0;
    }

#ifdef ANDROID
    playVideoAndroid(filename);
    return 0;
#endif

#ifdef IOS
    char *absolute_filename = new char[ strlen(archive_path) + strlen(filename) + 1 ];
    sprintf( absolute_filename, "%s%s", archive_path, filename );
    playVideoIOS(absolute_filename, click_flag, loop_flag);
    delete[] absolute_filename;
    return 0;
#endif

    int ret = 0;
#if defined(USE_SMPEG) && !defined(USE_SDL_RENDERER)
    unsigned char *mpeg_buffer = new unsigned char[length];
    script_h.cBR->getFile( filename, mpeg_buffer );
    SMPEG *mpeg_sample = SMPEG_new_rwops( SDL_RWFromMem( mpeg_buffer, length ), NULL, 0 );

    if ( !SMPEG_error( mpeg_sample ) ){
        SMPEG_enableaudio( mpeg_sample, 0 );

        if ( audio_open_flag ){
            SMPEG_actualSpec( mpeg_sample, &audio_format );
            SMPEG_enableaudio( mpeg_sample, 1 );
        }
        SMPEG_enablevideo( mpeg_sample, 1 );
        SMPEG_setdisplay( mpeg_sample, screen_surface, NULL, NULL );
        SMPEG_setvolume( mpeg_sample, music_volume );
        SMPEG_loop(mpeg_sample, loop_flag);

        Mix_HookMusic( mp3callback, mpeg_sample );
        SMPEG_play( mpeg_sample );

        bool done_flag = false;
        while( !(done_flag & click_flag) && SMPEG_status(mpeg_sample) == SMPEG_PLAYING ){
            SDL_Event event;

            while( SDL_PollEvent( &event ) ){
                switch (event.type){
                  case SDL_KEYUP:
                    if ( ((SDL_KeyboardEvent *)&event)->keysym.sym == SDLK_RETURN ||
                         ((SDL_KeyboardEvent *)&event)->keysym.sym == SDLK_SPACE ||
                         ((SDL_KeyboardEvent *)&event)->keysym.sym == SDLK_ESCAPE )
                        done_flag = true;
                    if ( ((SDL_KeyboardEvent *)&event)->keysym.sym == SDLK_RCTRL)
                        ctrl_pressed_status &= ~0x01;
                    
                    if ( ((SDL_KeyboardEvent *)&event)->keysym.sym == SDLK_LCTRL)
                        ctrl_pressed_status &= ~0x02;
                    break;
                  case SDL_QUIT:
                    ret = 1;
                  case SDL_MOUSEBUTTONUP:
                    done_flag = true;
                    break;
                  default:
                    break;
                }
            }
            SDL_Delay( 10 );
        }

        SMPEG_stop( mpeg_sample );
        Mix_HookMusic( NULL, NULL );
        SMPEG_delete( mpeg_sample );

    }
    delete[] mpeg_buffer;
#else
    fprintf( stderr, "mpegplay command is disabled.\n" );
#endif

    return ret;
}

int ONScripter::playAVI( const char *filename, bool click_flag )
{
    unsigned long length = script_h.cBR->getFileLength( filename );
    if (length == 0){
        fprintf( stderr, " *** can't find file [%s] ***\n", filename );
        return 0;
    }

#ifdef ANDROID
    playVideoAndroid(filename);
    return 0;
#endif

#if defined(USE_AVIFILE) && !defined(USE_SDL_RENDERER)
    char *absolute_filename = new char[ strlen(archive_path) + strlen(filename) + 1 ];
    sprintf( absolute_filename, "%s%s", archive_path, filename );
    for ( unsigned int i=0 ; i<strlen( absolute_filename ) ; i++ )
        if ( absolute_filename[i] == '/' ||
             absolute_filename[i] == '\\' )
            absolute_filename[i] = DELIMITER;

    if ( audio_open_flag ) Mix_CloseAudio();

    AVIWrapper *avi = new AVIWrapper();
    if ( avi->init( absolute_filename, false ) == 0 &&
         avi->initAV( screen_surface, audio_open_flag ) == 0 ){
        if (avi->play( click_flag )) return 1;
    }
    delete avi;
    delete[] absolute_filename;

    if ( audio_open_flag ){
        Mix_CloseAudio();
        openAudio();
    }
#else
    fprintf( stderr, "avi command is disabled.\n" );
#endif

    return 0;
}

void ONScripter::stopBGM( bool continue_flag )
{
    removeBGMFadeEvent();
    if (timer_bgmfade_id) SDL_RemoveTimer( timer_bgmfade_id );
    timer_bgmfade_id = NULL;
    mp3fadeout_duration_internal = 0;

#ifdef USE_CDROM
    if ( cdaudio_flag && cdrom_info ){
        extern SDL_TimerID timer_cdaudio_id;

        if ( timer_cdaudio_id ){
            SDL_RemoveTimer( timer_cdaudio_id );
            timer_cdaudio_id = NULL;
        }
        if (SDL_CDStatus( cdrom_info ) >= CD_PLAYING )
            SDL_CDStop( cdrom_info );
    }
#endif

    if ( wave_sample[MIX_BGM_CHANNEL] ){
        Mix_Pause( MIX_BGM_CHANNEL );
        Mix_FreeChunk( wave_sample[MIX_BGM_CHANNEL] );
        wave_sample[MIX_BGM_CHANNEL] = NULL;
    }

    if ( gbk_music_open ){
        ext_music_play_once_flag = true;
        Mix_HookMusic( NULL, NULL );
        gbkCloseOggMusic();
    }

    if ( music_info ){
        ext_music_play_once_flag = true;
        Mix_HaltMusic();
        Mix_FreeMusic( music_info );
        music_info = NULL;
    }

    if ( midi_info ){
        ext_music_play_once_flag = true;
        Mix_HaltMusic();
        Mix_FreeMusic( midi_info );
        midi_info = NULL;
    }

    if ( !continue_flag ){
        setStr( &music_file_name, NULL );
        music_play_loop_flag = false;
        if (music_buffer){
            delete[] music_buffer;
            music_buffer = NULL;
        }

        setStr( &midi_file_name, NULL );
        midi_play_loop_flag = false;

        current_cd_track = -1;
    }
}

void ONScripter::stopAllDWAVE()
{
    for (int ch=0; ch<ONS_MIX_CHANNELS ; ch++)
        if ( wave_sample[ch] ){
            Mix_Pause( ch );
            Mix_FreeChunk( wave_sample[ch] );
            wave_sample[ch] = NULL;
        }
}

void ONScripter::playClickVoice()
{
    if      ( clickstr_state == CLICK_NEWPAGE ){
        if ( clickvoice_file_name[CLICKVOICE_NEWPAGE] )
            playSound(clickvoice_file_name[CLICKVOICE_NEWPAGE], 
                      SOUND_CHUNK, false, MIX_WAVE_CHANNEL);
    }
    else if ( clickstr_state == CLICK_WAIT ){
        if ( clickvoice_file_name[CLICKVOICE_NORMAL] )
            playSound(clickvoice_file_name[CLICKVOICE_NORMAL], 
                      SOUND_CHUNK, false, MIX_WAVE_CHANNEL);
    }
}
