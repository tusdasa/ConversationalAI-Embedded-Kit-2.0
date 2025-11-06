// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __VOLC_HAL_H__
#define __VOLC_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the HAL module
 * 
 * @return int 0 if success, otherwise error code
 */
int volc_hal_init(void);

/**
 * @brief Deinitialize the HAL module
 */
void volc_hal_fini(void);

#ifdef __cplusplus
}
#endif

#endif