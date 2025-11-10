// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "audio_pipeline.h"
#include "audio_recorder.h"

#ifdef __cplusplus
extern "C"
{
#endif


    struct recorder_pipeline_t
    {
        audio_pipeline_handle_t audio_pipeline;
        audio_element_handle_t i2s_stream_reader;
        audio_element_handle_t  audio_encoder;
        audio_element_handle_t raw_reader;
        audio_rec_handle_t      recorder_engine;
        void                    *recorder_encoder;
        audio_element_handle_t rsp;
        audio_element_handle_t algo_aec;
    };

    typedef enum {
        WAKE_UP = 0,
        RECORD,
    } recorder_pipeline_type;

    typedef struct recorder_pipeline_t recorder_pipeline_t, *recorder_pipeline_handle_t;
    recorder_pipeline_handle_t recorder_pipeline_open();
    void recorder_pipeline_link_update(recorder_pipeline_handle_t handle,recorder_pipeline_type type);

    void recorder_pipeline_run(recorder_pipeline_handle_t);
    void recorder_pipeline_close(recorder_pipeline_handle_t);
    int recorder_pipeline_get_default_read_size(recorder_pipeline_handle_t);
    int recorder_pipeline_read(recorder_pipeline_handle_t, char *buffer, int buf_size);

    int local_recorder_pipeline_get_default_read_size(recorder_pipeline_handle_t pipeline);

    void *audio_record_engine_init(recorder_pipeline_handle_t pipeline, rec_event_cb_t cb);
    void audio_record_engine_deinit(recorder_pipeline_handle_t pipeline);

    esp_err_t recorder_pipeline_stop(recorder_pipeline_handle_t pipeline);

    struct player_pipeline_t
    {
        audio_pipeline_handle_t audio_pipeline;
        audio_element_handle_t raw_writer;
        audio_element_handle_t rsp;
        audio_element_handle_t i2s_stream_writer;
    };
    typedef struct player_pipeline_t player_pipeline_t, *player_pipeline_handle_t;
    player_pipeline_handle_t player_pipeline_open();
    void player_pipeline_run(player_pipeline_handle_t);
    void player_pipeline_close(player_pipeline_handle_t);
    int player_pipeline_get_default_read_size(player_pipeline_handle_t);
    int player_pipeline_write(player_pipeline_handle_t, char *buffer, int buf_size);
    void player_pipeline_write_play_buffer_flag(player_pipeline_handle_t player_pipeline);

#ifdef __cplusplus
}
#endif
#endif