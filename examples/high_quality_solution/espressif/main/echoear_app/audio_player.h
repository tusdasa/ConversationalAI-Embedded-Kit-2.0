#ifndef AUDIO_PLAYER_H__
#define AUDIO_PLAYER_H__

typedef void* audio_player_handle;
audio_player_handle audio_player_create(int channel_num,int samplerate);
int audio_player_init(audio_player_handle player);

int play_audio(audio_player_handle player,const void *data_ptr, int data_len);
int audio_player_destroy(audio_player_handle player);

#endif