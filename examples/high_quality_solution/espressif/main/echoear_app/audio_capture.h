#ifndef AUDIO_CAPTURE_H__
#define AUDIO_CAPTURE_H__

typedef void* audio_capture_handle;
audio_capture_handle audio_capture_create(int channel_num,int samplerate);
int audio_capture_init(audio_capture_handle capture);

int capture_audio(audio_capture_handle capture,const void *data_ptr, int data_len);
int audio_capture_destroy(audio_capture_handle capture);

#endif