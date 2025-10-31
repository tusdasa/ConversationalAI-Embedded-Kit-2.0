#include "iot_local_function_list.h"
#include "audio_vol.h"
void up_audio_val(){
    extern int audio_vol;
    extern audio_vol_handle vol_handle;
    audio_vol = (audio_vol + 10) <= 100 ? (audio_vol + 10) : 100;
    set_audio_val(vol_handle,audio_vol);
}

void down_audio_val(){
    extern int audio_vol;
    extern audio_vol_handle vol_handle;
    audio_vol = (audio_vol - 10) <= 0 ?  0 : (audio_vol - 10);
    set_audio_val(vol_handle,audio_vol);
}
