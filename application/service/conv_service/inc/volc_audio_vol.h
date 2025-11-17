#ifndef VOLC_AUDIO_VOL_H__
#define VOLC_AUDIO_VOL_H__

typedef void* volc_audio_vol_handle;
volc_audio_vol_handle volc_audio_vol_handle_create();
int volc_set_audio_val(volc_audio_vol_handle handle,int vol);

#endif
