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
typedef void* volc_capture_t;

typedef void (*volc_capture_data_cb_t)(volc_capture_t capture, const void* data, int len, void* user_data);

typedef struct volc_capture_config {
    volc_media_type_e media_type;   // Media type
    volc_capture_data_cb_t data_cb; // Data callback function
    void* user_data;               // User data pointer
} volc_capture_config_t;


/**
 * @brief Create a capture instance
 * 
 * @param config Capture configuration pointer
 * @return volc_capture_t Capture instance pointer
 */
volc_capture_t volc_capture_create(volc_capture_config_t* config);

/**
 * @brief Destroy a capture instance
 * 
 * @param capture Capture instance pointer
 */
void volc_capture_destroy(volc_capture_t capture);

/**
 * @brief Start a capture instance
 * 
 * @param capture Capture instance pointer
 * @return int 0 if success, otherwise error code
 */
int volc_capture_start(volc_capture_t capture);

/**
 * @brief Stop a capture instance
 * 
 * @param capture Capture instance pointer
 * @return int 0 if success, otherwise error code
 */
int volc_capture_stop(volc_capture_t capture);

#ifdef __cplusplus
}
#endif

#endif
