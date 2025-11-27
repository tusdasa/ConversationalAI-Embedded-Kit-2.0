// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include "iot_button.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_task_info.h"
#include "esp_random.h"
#include "esp_sntp.h"

#include "echoear_app/button_event.h"
#include "audio_recorder.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_sys.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "cJSON.h"
#include "network.h"
#include "volc_service_manager.h"
#include "volc_service_common.h"
#include "volc_conv_service_manager.h"
#include "volc_function_call_service.h"

#include "volc_hal.h"
#include "volc_hal_display.h"
#include "volc_hal_capture.h"
#include "volc_lvgl_source.h"
#include "board.h"
#include "esp_mac.h"

#define STATS_TASK_PRIO 5

static const char *TAG = "VolcConvAI";
static  int wake_up_running = 1;

void session_init(){
    volc_service_manager_init();
    volc_conv_service_manager_init();
    function_call_service_init();
}

void Task(void* data){
    aios_init(VOLC_SERVICE_EVENT_MAX);                            // AIOS初始化
    session_init();

    // aios_led_init();                                 // LED状态机初始化
    aios_run();                                      // AIOS启动
    // aios_run();
    return;
}

static void sys_monitor_task(void *pvParameters)
{
    static char run_info[1024] = {0};
    while (1)
    {
        // vTaskGetRunTimeStats(run_info);
        vTaskList(run_info);
        printf("Task Runtime Stats:\n%s", run_info);
        printf("--------------------------------\n");
        // ESP_LOGI(TAG, "Task Runtime Stats:\n%s", run_info);
        // ESP_LOGI(TAG, "--------------------------------\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");

    // 设置SNTP操作模式为轮询（客户端）
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // 设置NTP服务器（可以设置多个备用）
    esp_sntp_setservername(0, "pool.ntp.org");    // 主服务器
    esp_sntp_setservername(1, "cn.pool.ntp.org"); // 备用服务器：中国区
    esp_sntp_setservername(2, "ntp1.aliyun.com"); // 备用服务器：阿里云

    // 初始化SNTP服务
    esp_sntp_init();

    // 设置时区（例如北京时间CST-8）
    setenv("TZ", "CST-8", 1);
    tzset();
}

void wait_for_time_sync(void)
{
    ESP_LOGI(TAG, "Waiting for time synchronization...");

    // 检查时间是否已同步（1970年之后）
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry_count = 5; // 最大重试次数

    while (timeinfo.tm_year < (2024 - 1900) && retry_count > 0)
    {
        ESP_LOGI(TAG, "Time not set yet, retrying... (%d)", retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
        retry_count--;
    }

    if (retry_count > 0)
    {
        // print_current_time();
    }
    else
    {
        ESP_LOGE(TAG, "Failed to synchronize time within timeout period");
    }
}

static esp_err_t rec_engine_cb(audio_rec_evt_t *event, void *user_data)
{
    if (AUDIO_REC_WAKEUP_START == event->type) {
#if CONFIG_LANGUAGE_WAKEUP_MODE
        // printf("AUDIO_REC_WAKEUP_START \n");
        aios_event_pub(VOLC_SERVICE_AI_CONVERSATION,NULL,NULL);
#endif // CONFIG_LANGUAGE_WAKEUP_MODE
    } else if (AUDIO_REC_VAD_START == event->type) {
    } else if (AUDIO_REC_VAD_END == event->type) {
    } else if (AUDIO_REC_WAKEUP_END == event->type) {
    #if CONFIG_LANGUAGE_WAKEUP_MODE
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_WAKEUP_END");
    #endif // CONFIG_LANGUAGE_WAKEUP_MODE
    } else {
    }
    return ESP_OK;
}

static void wifi_ap_event_cb(void *arg, void *data)
{
    button_event_t btn_evt = iot_button_get_event(arg);
    if (btn_evt == BUTTON_LONG_PRESS_HOLD) {
        ESP_LOGW(TAG, "=== LONG PRESS DETECTED === Wi-Fi reset + restart");
        nvs_handle_t nvs;
        if (nvs_open("wifi", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_erase_key(nvs, "ssid");
            nvs_erase_key(nvs, "password");
            nvs_commit(nvs);
            nvs_close(nvs);
            ESP_LOGI(TAG, "Wi-Fi config erased from NVS");
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
        ESP_LOGI(TAG, "Restarting ESP32 now...");
        esp_restart();
    }
}

void  hal_level_init()
{

    // sntp init
    initialize_sntp();
    // wait for time sync
    wait_for_time_sync();
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    volc_hal_capture_config_t config = {0};
    config.media_type = VOLC_MEDIA_TYPE_AUDIO;
    config.data_cb = NULL;
    config.user_data = g_hal_context;
    config.audio_wakeup_cb = (volc_hal_audio_wakeup_cb_t)rec_engine_cb;
    volc_hal_capture_t capture = volc_hal_capture_create(&config);

    volc_hal_capture_start(capture,VOLC_AUDIO_MODE_WAKEUP);

    button_init();
    button_register_cb(6, BUTTON_LONG_PRESS_HOLD, wifi_ap_event_cb, NULL);

    // Allow other core to finish initialization
    // vTaskDelay(pdMS_TO_TICKS(2000));
}

void app_main(void)
{
    // xTaskCreate(&sys_monitor_task, "sys_monitor_task", 4096, NULL, 5, NULL);
    volc_hal_init();
    volc_hal_display_config_t display_config = {0};
    volc_hal_display_create(&display_config);

    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    if(g_hal_context == NULL){
        return;
    }
    volc_hal_display_t global_display = g_hal_context->display_handle;

    volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_MAIN,VOLC_DISPLAY_IMAGE,&img_app_pos);
    volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"初始化中...");
   
    /* Initialize the default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("*", ESP_LOG_INFO);

    // /* Initialize NVS flash for WiFi configuration */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());

    bool connected = configure_network();
    if (connected == false)
    {
        ESP_LOGE(TAG, "Failed to connect to network");
        return;
    }
    xTaskCreate(&Task, "conv_ai_task", 4096, NULL, 5, NULL);

    hal_level_init();
    volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"请说 hi 乐鑫,启动ai对话");
}
