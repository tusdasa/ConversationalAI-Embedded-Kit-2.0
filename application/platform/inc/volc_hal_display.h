// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __VOLC_HAL_DISPLAY_H__
#define __VOLC_HAL_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display instance pointer
 */
typedef void* volc_display_t;

typedef enum {
    VOLC_DISPLAY_OBJ_STATUS = 0, // Status display object
    VOLC_DISPLAY_OBJ_SUBTITLE,   // Subtitle display object
    VOLC_DISPLAY_OBJ_EMOJI,      // Emoji display object
    VOLC_DISPLAY_OBJ_MAX,        // Maximum display object count
} volc_display_obj_e;

typedef enum {
    VOLC_DISPLAY_TEXT = 0, // Text display type
    VOLC_DISPLAY_IMAGE,    // Image display type
    VOLC_DISPLAY_EMOJI     // Emoji display type
} volc_display_type_e;

typedef struct volc_display_config {
    int width;             // Width of the display
    int height;            // Height of the display
} volc_display_config_t;

/**
 * @brief Create a Display instance
 * 
 * @param config Display configuration
 * @return volc_display_t Display instance pointer
 */
volc_display_t volc_display_create(volc_display_config_t* config);

/**
 * @brief Destroy a Display instance
 * 
 * @param display Display instance pointer
 */
void volc_display_destroy(volc_display_t display);

/**
 * @brief Set the display emotion
 * 
 * @param display Display instance pointer
 * @param type Display type (VOLC_DISPLAY_TEXT, VOLC_DISPLAY_IMAGE, or VOLC_DISPLAY_EMOJI)
 * @param content Content to be displayed (string for text, image pointer for image, emoji string for emoji)
 * @return int 0 if success, otherwise error code
 */
int volc_display_set_content(volc_display_t display, volc_display_obj_e obj, volc_display_type_e type, const void* content);

#ifdef __cplusplus
}
#endif

#endif