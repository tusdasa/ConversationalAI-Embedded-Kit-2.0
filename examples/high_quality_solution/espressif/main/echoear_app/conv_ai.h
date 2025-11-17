#ifndef AIOS_CONV_AI_H__
#define AIOS_CONV_AI_H__
#include "volc_hal_capture.h"

void conv_ai_task(void *pvParameters);
void conv_ai_task_stop();
void conv_ai_init();
void audio_capture_cb(volc_hal_capture_t capture, const void* data, int len, volc_hal_frame_info_t* frame_info);

#endif