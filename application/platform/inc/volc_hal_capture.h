// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __VOLC_HAL_CAPTURE_H__
#define __VOLC_HAL_CAPTURE_H__

#include "volc_hal_type.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
 * @brief Capture instance pointer
 */ 
typedef void* volc_hal_capture_t;

typedef struct {
    volc_audio_data_type_e data_type; // Audio data type
    void* user_data;                  // User data pointer
} volc_hal_frame_info_t;

typedef enum {
    VOLC_AUDIO_MODE_WAKEUP = 0,
    VOLC_AUDIO_MODE_CAPTURE,
} volc_hal_audio_capture_mode_e;

typedef void (*volc_hal_capture_data_cb_t)(volc_hal_capture_t capture, const void* data, int len, volc_hal_frame_info_t* frame_info);
typedef int (*volc_hal_audio_wakeup_cb_t)(void *event, void *user_data);

typedef struct volc_hal_capture_config {
    volc_media_type_e media_type;   // Media type
    volc_hal_capture_data_cb_t data_cb; // Data callback function
    volc_hal_audio_wakeup_cb_t audio_wakeup_cb; // audio wakeup cb
    void* user_data;               // User data pointer
} volc_hal_capture_config_t;

/**
 * @brief Create a capture instance
 * 
 * @param config Capture configuration pointer
 * @return volc_hal_capture_t Capture instance pointer
 */
volc_hal_capture_t volc_hal_capture_create(volc_hal_capture_config_t* config);

/**
 * @brief Destroy a capture instance
 * 
 * @param capture Capture instance pointer
 */
void volc_hal_capture_destroy(volc_hal_capture_t capture);

/**
 * @brief Start a capture instance
 * 
 * @param capture Capture instance pointer
 * @return int 0 if success, otherwise error code
 */
int volc_hal_capture_start(volc_hal_capture_t capture,volc_hal_audio_capture_mode_e mode);

/**
 * @brief Stop a capture instance
 * 
 * @param capture Capture instance pointer
 * @return int 0 if success, otherwise error code
 */
int volc_hal_capture_stop(volc_hal_capture_t capture);

/**
 * @brief Update audio capture configuration
 * 
 * @param capture Capture instance pointer
 * @param config Capture configuration pointer
 * @return int 0 if success, otherwise error code
 */
int volc_hal_capture_update_config(volc_hal_capture_t capture, volc_hal_capture_config_t* config);

#ifdef __cplusplus
}
#endif

#endif
