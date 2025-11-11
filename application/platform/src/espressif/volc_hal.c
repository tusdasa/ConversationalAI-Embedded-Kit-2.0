// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "volc_hal.h"
#include "volc_osal.h"
#include "driver/gpio.h"

volc_hal_context_t* global_context = NULL;

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

int volc_hal_init(void) {
    if(global_context == NULL){
#if (CONFIG_ESP32_S3_ECHOEAR_V1_2_BOARD)
        __init_echoear_board_power();
#endif
        global_context = (volc_hal_context_t*) volc_osal_calloc(1,sizeof(volc_hal_context_t));
    }
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
