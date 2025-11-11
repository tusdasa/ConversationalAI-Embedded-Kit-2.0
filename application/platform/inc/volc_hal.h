// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __VOLC_HAL_H__
#define __VOLC_HAL_H__

#include "volc_hal_button.h"
#include "volc_hal_capture.h"
#include "volc_hal_display.h"
#include "volc_hal_player.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VOLC_HAL_CAPTURE_AUDIO = 0, // audio
    VOLC_HAL_CAPTURE_VIDEO,
    VOLC_HAL_CAPTURE_MAX,        // Maximum capture count
} volc_hal_capture_e;

typedef enum {
    VOLC_HAL_PLAYER_AUDIO = 0, // audio
    VOLC_HAL_PLAYER_VIDEO,
    VOLC_HAL_PLAYER_MAX,        // Maximum capture count
} volc_hal_player_e;

typedef struct {
    volc_hal_button_t button_handle;
    volc_hal_capture_t capture_handle[VOLC_HAL_CAPTURE_MAX];
    volc_hal_display_t display_handle;
    volc_hal_player_t player_handle[VOLC_HAL_PLAYER_MAX];
} volc_hal_context_t;

/**
 * @brief Initialize the HAL module
 * 
 * @return int 0 if success, otherwise error code
 */
int volc_hal_init(void);

/**
 * @brief return global hal context
 */
volc_hal_context_t* volc_get_global_hal_context();

/**
 * @brief Deinitialize the HAL module
 */
void volc_hal_fini(void);

#ifdef __cplusplus
}
#endif

#endif