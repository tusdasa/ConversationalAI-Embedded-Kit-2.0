// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "volc_hal_file.h"
#include "volc_hal.h"
#include "volc_osal.h"
#include "driver/gpio.h"
#include  "esp_mac.h"
#include <stdio.h>

#include "audio_processor.h"
#include "basic_board.h"

#define DEVICE_NAME_PREFIX "esp32_"

volc_hal_context_t* global_context = NULL;

audio_manager_config_t global_audio_manager_config;
basic_board_periph_t global_periph = {0};

#if (CONFIG_ESP32_S3_ECHOEAR_V1_2_BOARD)
static void __init_echoear_board_power(void)
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
#endif

static void __gmf_audio_init()
{
    audio_manager_config_t config = DEFAULT_AUDIO_MANAGER_CONFIG();
    global_audio_manager_config = config;
    basic_board_init(&global_periph);
    global_audio_manager_config.play_dev = global_periph.play_dev;
    global_audio_manager_config.rec_dev = global_periph.rec_dev;
    strcpy(global_audio_manager_config.mic_layout, global_periph.mic_layout);
    global_audio_manager_config.board_sample_rate = global_periph.sample_rate;
    global_audio_manager_config.board_bits = global_periph.sample_bits;
    global_audio_manager_config.board_channels = global_periph.channels;
    audio_manager_init(&global_audio_manager_config);
}

static void __generate_device_name(char* device_name){
    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    // printf("WIFI_STA MAC 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
    //           mac[0], mac[1], mac[2],
    //           mac[3], mac[4], mac[5]);
    snprintf(device_name,VOLC_DEVICE_NAME_LENGTH,"%s%02X%02X%02X%02X%02X%02X",DEVICE_NAME_PREFIX,mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int volc_hal_init(void) {
    if(global_context == NULL){
#if (CONFIG_ESP32_S3_ECHOEAR_V1_2_BOARD)
        // __init_echoear_board_power();
#endif
        global_context = (volc_hal_context_t*) volc_osal_calloc(1,sizeof(volc_hal_context_t));
    }
    __gmf_audio_init();
    volc_hal_file_system_init();
    __generate_device_name(global_context->device_name);
    return 0;
}

volc_hal_context_t* volc_get_global_hal_context(){
    return global_context;
}

void volc_hal_fini(void) {
    volc_osal_free(global_context);
    global_context = NULL;
    return;
}
