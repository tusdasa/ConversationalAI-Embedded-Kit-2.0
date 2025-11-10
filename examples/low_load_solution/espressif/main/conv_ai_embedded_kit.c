// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

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
#include "board.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "volc_hal_capture.h"
#include "volc_hal_player.h"
#include "cJSON.h"
#include "network.h"
#include "volc_conv_ai.h"
#define PRINT_TASK_INFO 0

// #include "iot_button.h"
// #include "button_gpio.h"
#include "volc_hal_button.h"
#define STATS_TASK_PRIO 5

static const char *TAG = "VolcConvAI";
static volatile bool is_interrupt = false;

#define CONV_AI_CONFIG_FORMAT "{\
  \"ver\": 1,\
  \"iot\": {\
    \"instance_id\": \"%s\",\
    \"product_key\": \"%s\",\
    \"product_secret\": \"%s\",\
    \"device_name\": \"%s\"\
  },\
  \"ws\": {\
    \"audio\": {\
      \"codec\": %d\
    }\
  }\
}"

typedef struct
{
    volc_hal_player_t player;
    volc_event_handler_t volc_event_handler;
    volc_engine_t engine;
} engine_context_t;

static char config_buf[1024] = {0};
static char config_audio[256] = {0};
static engine_context_t engine_ctx = {0};
static bool is_ready = false;

static volc_button_t button = NULL;

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

static void _on_volc_event(volc_engine_t handle, volc_event_t *event, void *user_data)
{
    switch (event->code)
    {
    case VOLC_EV_CONNECTED:
        is_ready = true;
        ESP_LOGI(TAG, "Volc Engine connected\n");
        break;
    case VOLC_EV_DISCONNECTED:
        is_ready = false;
        ESP_LOGI(TAG, "Volc Engine disconnected\n");
        break;
    default:
        ESP_LOGI(TAG, "Volc Engine event: %d\n", event->code);
        break;
    }
}

static void _on_volc_conversation_status(volc_engine_t handle, volc_conv_status_e status, void *user_data)
{
    ESP_LOGI(TAG, "conversation status changed: %d\n", status);
}

static void _on_volc_audio_data(volc_engine_t handle, const void *data_ptr, size_t data_len, volc_audio_frame_info_t *info_ptr, void *user_data)
{
    int error = 0;
    engine_context_t *demo = (engine_context_t *)user_data;
    if (demo == NULL)
    {
        ESP_LOGE(TAG, "demo is NULL\n");
        return;
    }
    if (demo->player == NULL)
    {
        ESP_LOGE(TAG, "player is NULL\n");
        return;
    }
    volc_hal_player_play_data(demo->player, data_ptr, data_len);
}

static void _on_volc_video_data(volc_engine_t handle, const void *data_ptr, size_t data_len, volc_video_frame_info_t *info_ptr, void *user_data)
{
}

static void _on_volc_message_data(volc_engine_t handle, const void *message, size_t size, volc_message_info_t *info_ptr, void *user_data)
{
    ESP_LOGI(TAG, "Received message: %.*s", (int)size, (const char *)message);
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

void print_current_time(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    time(&now);
    localtime_r(&now, &timeinfo);

    // 格式化输出时间
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", strftime_buf);
}

void wait_for_time_sync(void)
{
    ESP_LOGI(TAG, "Waiting for time synchronization...");

    // 检查时间是否已同步（1970年之后）
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry_count = 15; // 最大重试次数

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
        print_current_time();
    }
    else
    {
        ESP_LOGE(TAG, "Failed to synchronize time within timeout period");
    }
}

static void wifi_ap_event_cb(volc_button_t button, volc_button_event_e event, void* user_data)
{

    if (event == VOLC_BUTTON_LONG_PRESS_HOLD) {
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
// static void button_init(void)
// {
//     button_config_t btn_config = { 0 };
//     btn_config.type = BUTTON_TYPE_GPIO;
//     btn_config.gpio_button_config.gpio_num = GPIO_NUM_0;
//     btn_config.gpio_button_config.active_level = 0;
    
//     button_handle_t btn = NULL;
//     btn = iot_button_create(&btn_config);
//     iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, wifi_ap_event_cb, NULL);
// }

static void sys_monitor_task(void *pvParameters)
{
    static char run_info[1024] = {0};
    while (1)
    {
        vTaskGetRunTimeStats(run_info);
        ESP_LOGI(TAG, "Task Runtime Stats:\n%s", run_info);
        ESP_LOGI(TAG, "--------------------------------\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static uint64_t __get_time_ms(void)
{
    struct timespec now_time;
    clock_gettime(CLOCK_REALTIME, &now_time);
    return now_time.tv_sec * 1000 + now_time.tv_nsec / 1000000;
}

static int __volc_audio_codec(void) {
#if (CONFIG_VOLC_AUDIO_G711A)
    return VOLC_AUDIO_CODEC_TYPE_G711A;
#else
    return VOLC_AUDIO_CODEC_TYPE_PCM;
#endif
}

void volc_capture_audio_data(volc_capture_t capture, const void* data, int len, volc_frame_info_t* frame_info)
{
    // TODO: add audio data processing logic
    volc_audio_frame_info_t info = {0};
    info.data_type = frame_info->data_type;
    info.commit = false;
    volc_send_audio_data(engine_ctx.engine, data, len, &info);
}

static void conv_ai_task(void *pvParameters)
{
    int audio_codec = __volc_audio_codec();
    int error = 0;
    // step 1: start audio capture & play
    volc_capture_config_t capture_config = {
        .media_type = VOLC_MEDIA_TYPE_AUDIO,
        .data_cb = volc_capture_audio_data,
        .user_data = &engine_ctx,
    };
    volc_capture_t pipeline = volc_capture_create(&capture_config);
    volc_hal_player_config_t player_config = {
        .media_type = VOLC_MEDIA_TYPE_AUDIO,
    };
    volc_hal_player_t player = volc_hal_player_create(&player_config);
    volc_hal_player_start(player);

    // step 2: create ai agent
    snprintf(config_buf, sizeof(config_buf), CONV_AI_CONFIG_FORMAT,
             CONFIG_VOLC_INSTANCE_ID,
             CONFIG_VOLC_PRODUCT_KEY,
             CONFIG_VOLC_PRODUCT_SECRET,
             CONFIG_VOLC_DEVICE_NAME,
             audio_codec);
    ESP_LOGI(TAG, "conv ai config: %s", config_buf);
    volc_event_handler_t volc_event_handler = {
        .on_volc_event = _on_volc_event,
        .on_volc_conversation_status = _on_volc_conversation_status,
        .on_volc_audio_data = _on_volc_audio_data,
        .on_volc_video_data = _on_volc_video_data,
        .on_volc_message_data = _on_volc_message_data,
    };
    engine_ctx.volc_event_handler = volc_event_handler;
    engine_ctx.player = player;
    error = volc_create(&engine_ctx.engine, config_buf, &engine_ctx.volc_event_handler, &engine_ctx);
    if (error != 0)
    {
        ESP_LOGE(TAG, "Failed to create volc engine! error=%d", error);
        return;
    }

    // step 3: start ai agent
    volc_opt_t opt = {
        .mode = VOLC_MODE_WS,
        .bot_id = CONFIG_VOLC_BOT_ID,
        .params = NULL,
    };
    error = volc_start(engine_ctx.engine, &opt);
    if (error != 0)
    {
        ESP_LOGE(TAG, "Failed to start volc engine! error=%d", error);
        volc_destroy(engine_ctx.engine);
        return;
    }

    // int read_size = volc_capture_get_default_read_size(pipeline);
    // uint8_t *audio_buffer = heap_caps_malloc(read_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // if (!audio_buffer)
    // {
    //     ESP_LOGE(TAG, "Failed to alloc audio buffer!");
    //     return;
    // }
    // step 4: start sending audio data
//     volc_audio_frame_info_t info = {0};
// #if (CONFIG_VOLC_AUDIO_G711A)
//     info.data_type = VOLC_AUDIO_DATA_TYPE_G711A;
// #else
//     info.data_type = VOLC_AUDIO_DATA_TYPE_PCM;
// #endif
//     info.commit = false;
    // while (1)
    // {
    //     int ret = volc_capture_read(pipeline, (char *)audio_buffer, read_size);
    //     if (ret == read_size && is_ready)
    //     {
    //         // push_audio data
    //         volc_send_audio_data(engine_ctx.engine, audio_buffer, read_size, &info);
    //     }
    // }
    volc_capture_start(pipeline);
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    volc_button_destroy(button);

    // step 5: stop audio capture
    volc_capture_destroy(pipeline);

    // step 6: stop and destroy engine
    volc_stop(engine_ctx.engine);
    volc_destroy(engine_ctx.engine);

    // step 7: stop audio play
    volc_hal_player_destroy(player);
    vTaskDelete(NULL);
}

void app_main(void)
{
    init_echoear_board_power();
    /* Initialize the default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    /* Initialize NVS flash for WiFi configuration */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    // button
    volc_button_config_t button_config = {0};
    button_config.gpio_num = GPIO_NUM_0;
    button_config.active_level = 0;
    button_config.event_cb = wifi_ap_event_cb;
    button_config.user_data = &engine_ctx;
    button = volc_button_create(&button_config);
    if (NULL == button) {
        ESP_LOGE(TAG, "Failed to create button!");
        return;
    }

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    bool connected = configure_network();
    if (connected == false)
    {
        ESP_LOGE(TAG, "Failed to connect to network");
        return;
    }

    // sntp init
    initialize_sntp();
    // wait for time sync
    wait_for_time_sync();

    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 50);
    ESP_LOGI(TAG, "Starting again!\n");

    // Allow other core to finish initialization
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Create and start stats task
    xTaskCreate(&conv_ai_task, "conv_ai_task", 8192, NULL, STATS_TASK_PRIO, NULL);
#if (PRINT_TASK_INFO != 0)
    xTaskCreate(&sys_monitor_task, "sys_monitor_task", 4096, NULL, STATS_TASK_PRIO, NULL);
#endif
}
