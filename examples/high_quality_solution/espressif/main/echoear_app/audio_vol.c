#include "audio_vol.h"
#include "board.h"
int audio_vol = 70;
audio_vol_handle vol_handle = NULL;

audio_vol_handle audio_vol_handle_create(){
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    return board_handle;
}

int set_audio_val(audio_vol_handle handle,int vol){
    audio_board_handle_t board_handle = (handle);
    return audio_hal_set_volume(board_handle->audio_hal, vol);
}