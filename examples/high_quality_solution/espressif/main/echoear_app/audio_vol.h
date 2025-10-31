#ifndef AUDIO_VOL_H__
#define AUDIO_VOL_H__

typedef void* audio_vol_handle;
audio_vol_handle audio_vol_handle_create();
int set_audio_val(audio_vol_handle handle,int vol);

#endif