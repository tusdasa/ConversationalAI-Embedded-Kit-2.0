#include "audio_capture.h"
#include "pipeline.h"

audio_capture_handle audio_capture_create(int channel_num,int samplerate){
    return recorder_pipeline_open();
}

int audio_capture_init(audio_capture_handle capture){
    recorder_pipeline_run(capture);
    return 0;
}

int capture_audio(audio_capture_handle capture,const void *data_ptr, int data_len){
    return recorder_pipeline_read(capture, (char *)data_ptr, data_len);
}
int audio_capture_destroy(audio_capture_handle capture){
    recorder_pipeline_close(capture);
    return 0;
}
