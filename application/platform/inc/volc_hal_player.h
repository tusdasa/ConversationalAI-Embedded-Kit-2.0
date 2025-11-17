// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __VOLC_HAL_PLAYER_H__
#define __VOLC_HAL_PLAYER_H__

#include "volc_hal_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Player instance pointer
 */
typedef void* volc_hal_player_t;

typedef struct volc_hal_player_config {
    volc_media_type_e media_type;   // Media type
} volc_hal_player_config_t;

/**
 * @brief Create a player instance
 * 
 * @param config Player configuration
 * @return volc_hal_player_t Player instance pointer
 */
volc_hal_player_t volc_hal_player_create(volc_hal_player_config_t* config);

/**
 * @brief set audio player volume
 * 
 * @param player Player instance pointer (audio)
 * @param volume the volume [0,100]
 * @return volc_hal_player_t Player instance pointer
 */
int volc_hal_set_audio_player_volume(volc_hal_player_t player, int volume);

/**
 * @brief Destroy a player instance
 * 
 * @param player Player instance pointer
 */
void volc_hal_player_destroy(volc_hal_player_t player);

/**
 * @brief Start a player instance
 * 
 * @param player Player instance pointer
 * @return int 0 if success, otherwise error code
 */
int volc_hal_player_start(volc_hal_player_t player);

/**
 * @brief Stop a player instance
 * 
 * @param player Player instance pointer
 * @return int 0 if success, otherwise error code
 */
int volc_hal_player_stop(volc_hal_player_t player);

/**
 * @brief Play data with a player instance
 * 
 * @param player Player instance pointer
 * @param data Data to play
 * @param len Data length
 * @return int 0 if success, otherwise error code
 */
int volc_hal_player_play_data(volc_hal_player_t player, const void* data, int len);

#ifdef __cplusplus
}
#endif

#endif
