#include "volc_local_function_list.h"
#include "volc_audio_vol.h"

extern int volc_audio_vol;
extern volc_audio_vol_handle volc_vol_handle;

void volc_up_audio_val()
{
    volc_audio_vol = (volc_audio_vol + 10) <= 100 ? (volc_audio_vol + 10) : 100;
    volc_set_audio_val(volc_vol_handle,volc_audio_vol);
}

void volc_down_audio_val()
{
    volc_audio_vol = (volc_audio_vol - 10) <= 0 ?  0 : (volc_audio_vol - 10);
    volc_set_audio_val(volc_vol_handle,volc_audio_vol);
}
