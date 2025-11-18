// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>

#include "volc_hal.h"
#include "volc_hal_player.h"
#include "volc_osal.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "board.h"
#include "raw_stream.h"
#include "algorithm_stream.h"
#include "filter_resample.h"
#include "i2s_stream.h"
#include "util/volc_log.h"
#if defined(CONFIG_VOLC_AUDIO_G711A)
#include "g711_decoder.h"
#endif

#define CHANNEL 1
#define DEFAULT_AUDIO_VOL 50
static const char *TAG = "volc_player";
#define I2S_SAMPLE_RATE 16000
#define ALGO_SAMPLE_RATE 16000
#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD || CONFIG_ESP32_S3_ECHOEAR_V1_2_BOARD)
#define ALGORITHM_STREAM_SAMPLE_BIT 32
#define CHANNEL_FORMAT I2S_CHANNEL_TYPE_ONLY_LEFT
#define ALGORITHM_INPUT_FORMAT "RM"
#define CHANNEL_NUM 1
#elif CONFIG_M5STACK_ATOMS3R_BOARD
#define ALGORITHM_STREAM_SAMPLE_BIT 16
#define CHANNEL_FORMAT I2S_CHANNEL_TYPE_RIGHT_LEFT
#define ALGORITHM_INPUT_FORMAT "MR"
#define CHANNEL_NUM 2
#endif
#if (CONFIG_VOLC_AUDIO_G711A)
#define CODEC_NAME          "g711a"
#define CODEC_SAMPLE_RATE   8000
#else
#define CODEC_NAME        "pcm"
#define CODEC_SAMPLE_RATE 16000
#endif

typedef struct {
    audio_pipeline_handle_t audio_pipeline;
    audio_element_handle_t raw_writer;
    audio_element_handle_t audio_decoder;
    audio_element_handle_t rsp;
    audio_element_handle_t i2s_stream_writer;
    audio_board_handle_t board_handle;
    int volume;
} audio_player_config_t;

typedef struct {
    //TODO: add video player config
    void* video_player_config;
} video_player_config_t;

typedef struct {
    volc_media_type_e media_type;   // Media type
    union {
        audio_player_config_t audio_player_config;
        video_player_config_t video_player_config;
    };
} volc_player_impl_t;

static audio_element_handle_t __create_player_raw_stream(void)
{
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_cfg.out_rb_size = 8 * 1024;
    return raw_stream_init(&raw_cfg);
}

static audio_element_handle_t __create_player_i2s_stream(void)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, AUDIO_STREAM_WRITER);
    i2s_cfg.type = AUDIO_STREAM_WRITER;
#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD || CONFIG_ESP32_S3_ECHOEAR_V1_2_BOARD)
    i2s_cfg.need_expand = (16 != 32);
#endif
    i2s_cfg.out_rb_size = 8 * 1024;
    i2s_cfg.buffer_len = 1416; // 708
    i2s_stream_set_channel_type(&i2s_cfg, CHANNEL_FORMAT);
    audio_element_handle_t stream = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(stream, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, CHANNEL_NUM);
    return stream;
}

static audio_element_handle_t __create_resample_stream(int src_rate, int src_ch, int dest_rate, int dest_ch)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = src_rate;
    rsp_cfg.src_ch = src_ch;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_ch;
    rsp_cfg.complexity = 5;
    audio_element_handle_t stream = rsp_filter_init(&rsp_cfg);
    return stream;
}

static audio_element_handle_t __create_player_decoder_stream(void)
{
#if (CONFIG_VOLC_AUDIO_G711A)
    g711_decoder_cfg_t g711_dec_cfg = DEFAULT_G711_DECODER_CONFIG();
    g711_dec_cfg.out_rb_size = 8 * 1024;
    return g711_decoder_init(&g711_dec_cfg);
#else
    return NULL;
#endif
}

static audio_board_handle_t __create_player_audio_board(void)
{
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, DEFAULT_AUDIO_VOL);
    return board_handle;
}

volc_hal_player_t volc_hal_player_create(volc_hal_player_config_t* config)
{
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    if(g_hal_context == NULL){
        return NULL;
    }
    volc_player_impl_t* impl = volc_osal_calloc(1, sizeof(volc_player_impl_t));
    mem_assert(impl);
    impl->media_type = config->media_type;
    switch (impl->media_type) {
        case VOLC_MEDIA_TYPE_AUDIO:
            audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
            impl->audio_player_config.audio_pipeline = audio_pipeline_init(&pipeline_cfg);
            mem_assert(impl->audio_player_config.audio_pipeline);
            impl->audio_player_config.raw_writer = __create_player_raw_stream();
            audio_pipeline_register(impl->audio_player_config.audio_pipeline, impl->audio_player_config.raw_writer, "player_raw");
            impl->audio_player_config.i2s_stream_writer = __create_player_i2s_stream();
            audio_pipeline_register(impl->audio_player_config.audio_pipeline, impl->audio_player_config.i2s_stream_writer, "player_i2s");
            impl->audio_player_config.rsp = __create_resample_stream(CODEC_SAMPLE_RATE, 1, I2S_SAMPLE_RATE, CHANNEL_NUM);
            audio_element_set_output_timeout(impl->audio_player_config.rsp, portMAX_DELAY);
            audio_pipeline_register(impl->audio_player_config.audio_pipeline, impl->audio_player_config.rsp, "rsp");
            impl->audio_player_config.audio_decoder = __create_player_decoder_stream();
            if (impl->audio_player_config.audio_decoder != NULL) {
                audio_pipeline_register(impl->audio_player_config.audio_pipeline, impl->audio_player_config.audio_decoder, CODEC_NAME);
            }
            impl->audio_player_config.board_handle = __create_player_audio_board();
            impl->audio_player_config.volume = DEFAULT_AUDIO_VOL;
#if (CONFIG_VOLC_AUDIO_G711A)
            const char *link_tag[] = {"player_raw", CODEC_NAME, "rsp", "player_i2s"};
#else
            const char *link_tag[] = {"player_raw", "rsp", "player_i2s"};
#endif
            audio_pipeline_link(impl->audio_player_config.audio_pipeline, &link_tag[0], sizeof(link_tag) / sizeof(link_tag[0]));
            g_hal_context->player_handle[VOLC_HAL_PLAYER_AUDIO] = (volc_hal_player_t)impl;
            break;
        case VOLC_MEDIA_TYPE_VIDEO:
            // TODO: Add video player support
            break;
        default:
            break;
    }
    return (volc_hal_player_t)impl;
}

int volc_hal_set_audio_player_volume(volc_hal_player_t player, int volume){
    volc_player_impl_t* impl = (volc_player_impl_t*)player;
    if (impl == NULL) {
        return -1;
    }
    if(impl->media_type == VOLC_MEDIA_TYPE_AUDIO){
        impl->audio_player_config.volume = volume;
        return audio_hal_set_volume(impl->audio_player_config.board_handle->audio_hal, volume);
    } else if(impl->media_type == VOLC_MEDIA_TYPE_VIDEO){
        LOGE("media type %d not supported set volume", impl->media_type);
        return -1;
    }
}

int volc_hal_get_audio_player_volume(volc_hal_player_t player){
    volc_player_impl_t* impl = (volc_player_impl_t*)player;
    if (impl == NULL) {
        return -1;
    }
    return impl->audio_player_config.volume;
}

void volc_hal_player_destroy(volc_hal_player_t player)
{
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    if(g_hal_context == NULL){
        return;
    }
    volc_player_impl_t* impl = (volc_player_impl_t*)player;
    if (impl == NULL) {
        return;
    }
    switch (impl->media_type) {
        case VOLC_MEDIA_TYPE_AUDIO:
            audio_pipeline_stop(impl->audio_player_config.audio_pipeline);
            audio_pipeline_wait_for_stop(impl->audio_player_config.audio_pipeline);
            audio_pipeline_terminate(impl->audio_player_config.audio_pipeline);
            if (impl->audio_player_config.raw_writer)
            {
                audio_pipeline_unregister(impl->audio_player_config.audio_pipeline, impl->audio_player_config.raw_writer);
                audio_element_deinit(impl->audio_player_config.raw_writer);
            }
            if (impl->audio_player_config.rsp)
            {
                audio_pipeline_unregister(impl->audio_player_config.audio_pipeline, impl->audio_player_config.rsp);
                audio_element_deinit(impl->audio_player_config.rsp);
            }
            if (impl->audio_player_config.i2s_stream_writer)
            {
                audio_pipeline_unregister(impl->audio_player_config.audio_pipeline, impl->audio_player_config.i2s_stream_writer);
                audio_element_deinit(impl->audio_player_config.i2s_stream_writer);
            }
            if (impl->audio_player_config.audio_decoder) {
                audio_pipeline_unregister(impl->audio_player_config.audio_pipeline, impl->audio_player_config.audio_decoder);
                audio_element_deinit(impl->audio_player_config.audio_decoder);
            }
            audio_board_deinit(impl->audio_player_config.board_handle);
            audio_pipeline_deinit(impl->audio_player_config.audio_pipeline);
            g_hal_context->player_handle[VOLC_HAL_PLAYER_AUDIO] = NULL;
            break;
        case VOLC_MEDIA_TYPE_VIDEO:
            // TODO: Add video player support
            break;
        default:
            break;
    }
    volc_osal_free(impl);
}

int volc_hal_player_start(volc_hal_player_t player)
{
    volc_player_impl_t* impl = (volc_player_impl_t*)player;
    if (impl == NULL) {
        LOGE("volc_hal_player_start: impl is NULL");
        return -1;
    }
    switch (impl->media_type) {
        case VOLC_MEDIA_TYPE_AUDIO:
            return audio_pipeline_run(impl->audio_player_config.audio_pipeline);
        case VOLC_MEDIA_TYPE_VIDEO:
            // TODO: Add video player support
            break;
        default:
            break;
    }
    LOGE("media type %d not supported", impl->media_type);
    return -1;
}

int volc_hal_player_stop(volc_hal_player_t player)
{
    volc_player_impl_t* impl = (volc_player_impl_t*)player;
    if (impl == NULL) {
        LOGE("volc_hal_player_stop: impl is NULL");
        return -1;
    }
    switch (impl->media_type) {
        case VOLC_MEDIA_TYPE_AUDIO:
            return audio_pipeline_stop(impl->audio_player_config.audio_pipeline);
            break;
        case VOLC_MEDIA_TYPE_VIDEO:
            // TODO: Add video player support
            break;
        default:
            break;
    }
    LOGE("media type %d not supported", impl->media_type);
    return -1;
}

int volc_hal_player_play_data(volc_hal_player_t player, const void* data, int len)
{
    if (player == NULL || data == NULL || len <= 0) {
        LOGE("player play data failed, player is %p, data is %p, len is %d", player, data, len);
        return -1;
    }
    volc_player_impl_t* impl = (volc_player_impl_t*)player;
    switch (impl->media_type) {
        case VOLC_MEDIA_TYPE_AUDIO:
            return raw_stream_write(impl->audio_player_config.raw_writer, (char*)data, len);
            break;
        case VOLC_MEDIA_TYPE_VIDEO:
            // TODO: Add video player support
            break;
        default:
            break;
    }
    LOGE("media type %d not supported", impl->media_type);
    return -1;
}
