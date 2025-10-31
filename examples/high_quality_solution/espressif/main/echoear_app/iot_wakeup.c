#include "iot_wakeup.h"
#include "audio_processor.h"

recorder_pipeline_handle_t wake_record_pipeline = NULL; 
static void* recorder_engine= NULL; 
static  int running = 0;
static const char *TAG = "IOT_WakeUp";
iot_wakeup_cb g_wakeup_cb = NULL;
static void voice_read_task(void *args){
    const int voice_data_read_sz = local_recorder_pipeline_get_default_read_size(wake_record_pipeline);
    uint8_t *voice_data = heap_caps_malloc(voice_data_read_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    while (running) {
        // EventBits_t bits = xEventGroupWaitBits(s_volc_rtc.wakeup_event, WAKEUP_REC_READING , false, true, wait_tm);
            int ret = audio_recorder_data_read(recorder_engine, voice_data, voice_data_read_sz, portMAX_DELAY);
            if (ret == 0 || ret == -1) {
                if (ret == 0) {
                    vTaskDelay(15 / portTICK_PERIOD_MS);
                    continue;
                }
                // ESP_LOGE(TAG, "audio_recorder_data_read failed, ret: %d\n", ret);
            }
    }
    vTaskDelete(NULL);
}

void iot_wakeup_init(iot_wakeup_cb cb){
    wake_record_pipeline = local_recorder_pipeline_open();
    local_recorder_pipeline_run(wake_record_pipeline);
    if(cb){
        g_wakeup_cb = cb;
    }
    if(g_wakeup_cb){
        recorder_engine = audio_record_engine_init(wake_record_pipeline,(rec_event_cb_t)g_wakeup_cb);
    }

    // iot_display_string("请说: hi 乐鑫,唤醒ai对话");
    // int   ret = audio_thread_create(&s_volc_rtc.voice_read_task_handle, "voice_read_task", voice_read_task, (void *)NULL, 5 * 1024, 5, true, 0);
}

void iot_wakeup_start(){
    running = 1;
    xTaskCreate(&voice_read_task, "voice_read_task", 4096, NULL, 5, NULL);
}

void iot_wakeup_stop(){
    running = 0;
}

void iot_wakeup_deinit(){
    
    if(wake_record_pipeline){
        audio_record_engine_deinit(wake_record_pipeline);
   
        local_recorder_pipeline_stop(wake_record_pipeline);
  
        local_recorder_pipeline_close(wake_record_pipeline);
    }
    wake_record_pipeline = NULL;
}

void iot_wakeup_register_cb(iot_wakeup_cb cb){
    // recorder_engine = audio_record_engine_init(wake_record_pipeline,(rec_event_cb_t)cb);
    g_wakeup_cb = cb;
}