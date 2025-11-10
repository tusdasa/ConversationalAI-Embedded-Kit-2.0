// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#include "echoear_app/aios_app_manager.h"
#include "echoear_app/aios_ai_conversation_app.h"
#include "echoear_app/common_def.h"
#include "echoear_app/audio_vol.h"

#include "echoear_app/button_event.h"
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
#include "pipeline.h"
#include "cJSON.h"
#include "network.h"
#include "echoear_app/iot_display.h"
#include "echoear_app/conv_ai.h"

#include "echoear_app/iot_wakeup.h"
#include "driver/gpio.h"
#include "board.h"
#include  "esp_mac.h"
#define STATS_TASK_PRIO 5

static const char *TAG = "VolcConvAI";
extern audio_vol_handle vol_handle;
extern int audio_vol;


static recorder_pipeline_handle_t record_pipeline; 

extern recorder_pipeline_handle_t wake_record_pipeline; 

static  int wake_up_running = 1;

void session_init(){
    aios_app_manager_init();  
    aios_Ai_Conversation_app_init();
}

void Task(void* data){
    aios_init(Event_Max);                            // AIOS初始化
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


static void init_echoear_board_power(void)
{
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(GPIO_NUM_9) | BIT64(GPIO_NUM_48);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_9, 0);
    gpio_set_level(GPIO_NUM_48, 1);
}

// static void button_event_cb_set(void *arg,void *data)
// {
//     printf("按键被按下！%s \n",((char*)data));
//     aios_event_pub(Event_Ai_Conversation,NULL,NULL);
// }

// static void button_event_cb_play(void *arg,void *data)
// {
//     printf("按键被按下！%s \n",((char*)data));
//     aios_event_pub(Event_Ai_Conversation_QUIT,NULL,NULL);
// }

// static void button_event_cb_voladd(void *arg,void *data)
// {
//     printf("按键被按下！%s \n",((char*)data));
//     if(vol_handle){
//         audio_vol = (audio_vol + 10) <= 100 ? (audio_vol + 10) : 100;
//         set_audio_val(vol_handle,audio_vol);
//     }
    
// }

// static void button_event_cb_voldec(void *arg,void *data)
// {
//     printf("按键被按下！%s \n",((char*)data));
//     if(vol_handle){
//         audio_vol = (audio_vol - 10) >= 0 ? (audio_vol - 10) : 0;
//         set_audio_val(vol_handle,audio_vol);
//     }
// }

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
        aios_event_pub(Event_Ai_Conversation,NULL,NULL);
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

    vol_handle = audio_vol_handle_create();
    set_audio_val(vol_handle,audio_vol);
    iot_wakeup_init((iot_wakeup_cb)rec_engine_cb);
    iot_wakeup_start();

    button_init();
    button_register_cb(6, BUTTON_LONG_PRESS_HOLD, wifi_ap_event_cb, NULL);
    // Allow other core to finish initialization
    // vTaskDelay(pdMS_TO_TICKS(2000));
}

void app_main(void)
{
    // xTaskCreate(&sys_monitor_task, "sys_monitor_task", 4096, NULL, 5, NULL);
    
    init_echoear_board_power();
    iot_display_init();

    // 打印mac地址
    /*
    uint8_t derived_mac_addr[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(derived_mac_addr, ESP_MAC_WIFI_STA));
    printf("WIFI_STA MAC 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
              derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
              derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    */
   
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
    
    // 初始化sd卡 用于存储
    /*
        esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
        esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
        ESP_ERROR_CHECK(audio_board_sdcard_init(set, SD_MODE_1_LINE));
    */
 
    bool connected = configure_network();
    if (connected == false)
    {
        ESP_LOGE(TAG, "Failed to connect to network");
        return;
    }
    // button_init();
    // aios_init(Event_Max);                            // AIOS初始化
    xTaskCreate(&Task, "conv_ai_task", 4096, NULL, 5, NULL);
    // aios_event_pub(Event_Ai_Conversation,NULL,NULL);
    // button_register_cb(0,0,button_event_cb_voladd,"vol+");
    // button_register_cb(1,0,button_event_cb_voldec,"vol-");
    // button_register_cb(2,0,button_event_cb_set,"set");
    // button_register_cb(3,0,button_event_cb_play,"play");

    hal_level_init();
    // iot_wakeup_register_cb((iot_wakeup_cb)rec_engine_cb);
    iot_display_string("请说 hi 乐鑫,启动ai对话");
    // while(1){
    //     sleep(5);
    // }
    // sys_monitor_task(NULL);
}
