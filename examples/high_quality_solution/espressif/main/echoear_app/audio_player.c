#include "audio_player.h"
#include "pipeline.h"

audio_player_handle audio_player_create(int channel_num,int samplerate)
{
    return player_pipeline_open();
}

int audio_player_init(audio_player_handle player){
    player_pipeline_run((player_pipeline_handle_t)player);
    return 0;
}

int play_audio(audio_player_handle player,const void *data_ptr, int data_len){
   return player_pipeline_write((player_pipeline_handle_t)player, data_ptr, data_len);
}

int audio_player_destroy(audio_player_handle player){
    player_pipeline_close(player);
    return 0;
}