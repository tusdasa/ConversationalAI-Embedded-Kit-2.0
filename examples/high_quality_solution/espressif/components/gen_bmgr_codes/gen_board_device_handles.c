/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * Auto-generated device handle definition file
 * DO NOT MODIFY THIS FILE MANUALLY
 *
 * See LICENSE file for details.
 */

#include <stddef.h>
#include "esp_board_device.h"
#include "dev_audio_codec.h"
#include "dev_display_lcd.h"
#include "dev_fs_fat.h"
#include "dev_gpio_ctrl.h"
#include "dev_lcd_touch_i2c.h"
#include "dev_ledc_ctrl.h"
#include "dev_power_ctrl.h"

// Device handle array
esp_board_device_handle_t g_esp_board_device_handles[] = {
    {
        .next = &g_esp_board_device_handles[1],
        .name = "audio_power",
        .type = "power_ctrl",
        .device_handle = NULL,
        .init = dev_power_ctrl_init,
        .deinit = dev_power_ctrl_deinit
    },
    {
        .next = &g_esp_board_device_handles[2],
        .name = "lcd_sdcard_power",
        .type = "power_ctrl",
        .device_handle = NULL,
        .init = dev_power_ctrl_init,
        .deinit = dev_power_ctrl_deinit
    },
    {
        .next = &g_esp_board_device_handles[3],
        .name = "lcd_brightness",
        .type = "ledc_ctrl",
        .device_handle = NULL,
        .init = dev_ledc_ctrl_init,
        .deinit = dev_ledc_ctrl_deinit
    },
    {
        .next = &g_esp_board_device_handles[4],
        .name = "audio_dac",
        .type = "audio_codec",
        .device_handle = NULL,
        .init = dev_audio_codec_init,
        .deinit = dev_audio_codec_deinit
    },
    {
        .next = &g_esp_board_device_handles[5],
        .name = "audio_adc",
        .type = "audio_codec",
        .device_handle = NULL,
        .init = dev_audio_codec_init,
        .deinit = dev_audio_codec_deinit
    },
    {
        .next = &g_esp_board_device_handles[6],
        .name = "fs_sdcard",
        .type = "fs_fat",
        .device_handle = NULL,
        .init = dev_fs_fat_init,
        .deinit = dev_fs_fat_deinit
    },
    {
        .next = &g_esp_board_device_handles[7],
        .name = "display_lcd",
        .type = "display_lcd",
        .device_handle = NULL,
        .init = dev_display_lcd_init,
        .deinit = dev_display_lcd_deinit
    },
    {
        .next = &g_esp_board_device_handles[8],
        .name = "lcd_touch",
        .type = "lcd_touch_i2c",
        .device_handle = NULL,
        .init = dev_lcd_touch_i2c_init,
        .deinit = dev_lcd_touch_i2c_deinit
    },
    {
        .next = NULL,
        .name = "led_green",
        .type = "gpio_ctrl",
        .device_handle = NULL,
        .init = dev_gpio_ctrl_init,
        .deinit = dev_gpio_ctrl_deinit
    },
};
