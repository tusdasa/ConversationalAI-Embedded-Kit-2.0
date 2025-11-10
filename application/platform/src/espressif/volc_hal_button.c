// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>

#include "volc_hal_button.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "util/volc_log.h"
#include "volc_osal.h"

struct volc_button_impl_t {
    button_handle_t btn;
    volc_button_config_t config;
};

typedef struct volc_button_impl_t volc_button_impl_t;

static void button_event_cb_adapter(void* btn_handle, void* usr_data) {
    if (NULL == usr_data) {
        return;
    }

    struct {
        volc_button_impl_t* button_impl;
        button_event_t event;
    } *adapter_data = (void*)usr_data;

    if (NULL != adapter_data->button_impl && NULL != adapter_data->button_impl->config.event_cb) {
        adapter_data->button_impl->config.event_cb(
            (volc_button_t)adapter_data->button_impl,
            (volc_button_event_e)adapter_data->event,
            adapter_data->button_impl->config.user_data
        );
    }
}

volc_button_t volc_button_create(volc_button_config_t* config) {
    if (NULL == config) {
        LOGW("invalid input config %p", config);
        return NULL;
    }
    volc_button_impl_t *button_impl = (volc_button_impl_t *)volc_osal_malloc(sizeof(volc_button_impl_t));
    if (NULL == button_impl) {
        LOGE("memory alloc failed");
        return NULL;
    }
    button_impl->config.gpio_num = config->gpio_num;
    button_impl->config.active_level = config->active_level;
    button_impl->config.long_press_ms = config->long_press_ms;
    button_impl->config.short_press_ms = config->short_press_ms;
    button_impl->config.enable_power_save = config->enable_power_save;
    button_impl->config.user_data = config->user_data;
    button_impl->config.event_cb = config->event_cb;

    button_config_t btn_config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config.gpio_num = button_impl->config.gpio_num,
        .gpio_button_config.active_level = button_impl->config.active_level,
        .long_press_time = button_impl->config.long_press_ms,
        .short_press_time = button_impl->config.short_press_ms
    };

    button_impl->btn = iot_button_create(&btn_config);
    if (NULL == button_impl->btn) {
        LOGE("button create failed");
        volc_osal_free(button_impl);
        return NULL;
    }

    struct {
        volc_button_impl_t* button_impl;
        button_event_t event;
    } *adapter_data = NULL;

    button_event_t events[] = {
        BUTTON_PRESS_DOWN,
        BUTTON_PRESS_UP,
        BUTTON_PRESS_REPEAT,
        BUTTON_PRESS_REPEAT_DONE,
        BUTTON_SINGLE_CLICK,
        BUTTON_DOUBLE_CLICK,
        BUTTON_MULTIPLE_CLICK,
        BUTTON_LONG_PRESS_START,
        BUTTON_LONG_PRESS_HOLD,
        BUTTON_LONG_PRESS_UP,
        BUTTON_PRESS_END
    };

    for (int i = 0; i < sizeof(events)/sizeof(events[0]); i++) {
        adapter_data = (void*)volc_osal_malloc(sizeof(*adapter_data));
        if (NULL == adapter_data) {
            LOGE("Failed to allocate adapter data for event %d", events[i]);
            continue;
        }

        adapter_data->button_impl = button_impl;
        adapter_data->event = events[i];

        iot_button_register_cb(button_impl->btn, events[i], button_event_cb_adapter, adapter_data);
    }

    return (volc_button_t)button_impl;
}

void volc_button_destroy(volc_button_t button) {
    if (NULL == button) {
        LOGW("invalid input button %p", button);
        return;
    }
    volc_button_impl_t *button_impl = (volc_button_impl_t *)button;

    if (NULL != button_impl->btn) {
        iot_button_delete(button_impl->btn);
        button_impl->btn = NULL;
    }

    volc_osal_free(button_impl);
}