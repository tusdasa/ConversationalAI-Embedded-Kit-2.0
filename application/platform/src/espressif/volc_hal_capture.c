// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0
#include <stddef.h>

#include "audio_common.h"
#include "audio_sys.h"
#include "board.h"
#include "volc_hal.h"
#include "volc_hal_capture.h"
#include "volc_osal.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "raw_stream.h"
#include "algorithm_stream.h"
#include "filter_resample.h"
#include "i2s_stream.h"
#include "util/volc_log.h"
#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD || CONFIG_ESP32_S3_ECHOEAR_V1_2_BOARD)
#include "es7210.h"
#elif CONFIG_M5STACK_ATOMS3R_BOARD
#include "es8311.h"
#endif
#include "esp_timer.h"
#if defined(CONFIG_VOLC_AUDIO_G711A)
#include "g711_encoder.h"
#endif
#include "audio_idf_version.h"
#include "recorder_sr.h"
#include "audio_recorder.h"
#include "recorder_encoder.h"

#define CHANNEL 1
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

typedef enum {
    WAKEUP = 0,
    CAPTURE,
    INITED,
} audio_pipeline_state_e;

typedef struct {
    audio_pipeline_handle_t  audio_pipeline;
    audio_element_handle_t   i2s_stream_reader;
    audio_element_handle_t   audio_encoder;
    audio_element_handle_t   raw_reader;
    audio_element_handle_t   rsp;
    audio_element_handle_t   algo_aec;
    audio_rec_handle_t       recorder_engine;
    void                    *recorder_encoder;
    audio_pipeline_state_e   state;
} volc_hal_audio_capture_config_t;

typedef struct {
    //TODO: add video capture config
    void* video_capture_config;
} volc_hal_video_capture_config_t;

typedef struct {
    volc_media_type_e media_type;
    union {
        volc_hal_audio_capture_config_t audio_capture_config;
        volc_hal_video_capture_config_t video_capture_config;
    };
    volc_hal_capture_data_cb_t     data_cb; // Data callback function
    volc_hal_audio_wakeup_cb audio_wakeup_cb; // audio wakeup cb
    void*                      user_data;               // User data pointer
    volc_osal_tid_t            capture_thread;
    volatile bool              is_started;
} volc_hal_capture_impl_t;

static int __volc_capture_get_default_read_size(volc_hal_capture_t capture)
{
    // 60ms
#if (CONFIG_VOLC_AUDIO_G711A)
    return (160 * 3);
#else
    return 960;
#endif
}

static void __volc_audio_capture_task(void *arg)
{
    volc_hal_capture_impl_t *impl = (volc_hal_capture_impl_t *)arg;
    LOGI("capture task started...");
    int read_size = __volc_capture_get_default_read_size(impl->audio_capture_config.audio_pipeline);
    uint8_t *read_buf = volc_osal_malloc(read_size);
    mem_assert(read_buf);

    while (impl->is_started) {
        int ret = raw_stream_read(impl->audio_capture_config.raw_reader, read_buf, read_size);
        if (ret > 0 && impl->data_cb) {
        #ifdef CONFIG_VOLC_AUDIO_G711A
            volc_hal_frame_info_t frame_info = {
                .data_type = VOLC_AUDIO_DATA_TYPE_G711A,
                .user_data = impl->user_data,
            };
            impl->data_cb(impl, (const void *)read_buf, ret, &frame_info);
        #else
            volc_hal_frame_info_t frame_info = {
                .data_type = VOLC_AUDIO_DATA_TYPE_PCM,
                .user_data = impl->user_data,
            };
            impl->data_cb(impl, (const void *)read_buf, ret, &frame_info);
        #endif
        }
    }
    audio_pipeline_wait_for_stop(impl->audio_capture_config.audio_pipeline);
    if (impl->capture_thread) {
        volc_osal_thread_destroy(impl->capture_thread);
        impl->capture_thread = NULL;
    }
    volc_osal_free(read_buf);
    volc_osal_thread_exit(NULL);
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

static audio_element_handle_t __create_record_i2s_stream(bool enable_wake)
{
#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD || CONFIG_ESP32_S3_ECHOEAR_V1_2_BOARD)
    es7210_adc_set_gain(ES7210_INPUT_MIC3, GAIN_30DB);
#elif CONFIG_M5STACK_ATOMS3R_BOARD
    es8311_set_mic_gain(ES8311_MIC_GAIN_36DB);
#endif

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, AUDIO_STREAM_READER); // 参数需要仔细检查
    i2s_cfg.type = AUDIO_STREAM_READER;
    if(!enable_wake){
        i2s_stream_set_channel_type(&i2s_cfg, CHANNEL_FORMAT);
    }
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = I2S_SAMPLE_RATE;
    return i2s_stream_init(&i2s_cfg);
}

static audio_element_handle_t __create_record_encoder_stream(void)
{
#if(CONFIG_VOLC_AUDIO_G711A)
    g711_encoder_cfg_t g711_cfg = DEFAULT_G711_ENCODER_CONFIG();
    return g711_encoder_init(&g711_cfg);
#else
    return NULL;
#endif
}

static audio_element_handle_t __create_record_raw_stream(void)
{
    audio_element_handle_t raw_stream = NULL;
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_cfg.out_rb_size = 2 * 1024;
    raw_stream = raw_stream_init(&raw_cfg);
    audio_element_set_output_timeout(raw_stream, portMAX_DELAY);
    return raw_stream;
}

static audio_element_handle_t __create_record_algo_stream(void)
{
    LOGI("[3.1] Create algorithm stream for aec");
    algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
    algo_config.sample_rate = ALGO_SAMPLE_RATE;
    algo_config.out_rb_size = 256;
    algo_config.algo_mask = ALGORITHM_STREAM_DEFAULT_MASK | ALGORITHM_STREAM_USE_AGC;
    algo_config.input_format = ALGORITHM_INPUT_FORMAT;
    audio_element_handle_t element_algo = algo_stream_init(&algo_config);
    audio_element_set_music_info(element_algo, ALGO_SAMPLE_RATE, 1, 16);
    audio_element_set_input_timeout(element_algo, portMAX_DELAY);
    return element_algo;
}

static recorder_sr_cfg_t __get_default_audio_record_config(void)
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG(AUDIO_ADC_INPUT_CH_FORMAT, "model", AFE_TYPE_SR, AFE_MODE_LOW_COST);
    recorder_sr_cfg.multinet_init = false;
    recorder_sr_cfg.fetch_task_core = 0;
    recorder_sr_cfg.feed_task_core = 1;
    recorder_sr_cfg.afe_cfg->vad_mode = VAD_MODE_3;
    recorder_sr_cfg.afe_cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg->agc_mode = AFE_MN_PEAK_NO_AGC;
    return recorder_sr_cfg;
}

static int __input_cb_for_afe(int16_t *buffer, int buf_sz, void *user_ctx, TickType_t ticks)
{
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    if(g_hal_context == NULL || g_hal_context->capture_handle[VOLC_HAL_CAPTURE_AUDIO] == NULL){
        return -1;
    }
    volc_hal_capture_impl_t *capture = (volc_hal_capture_impl_t *)g_hal_context->capture_handle[VOLC_HAL_CAPTURE_AUDIO];
    return raw_stream_read(capture->audio_capture_config.raw_reader, (char *)buffer, buf_sz);
}


static audio_element_handle_t __create_wake_encoder_stream(void)
{
    audio_element_handle_t encoder_stream = NULL;
#if defined (CONFIG_AUDIO_SUPPORT_OPUS_DECODER)
    raw_opus_enc_config_t opus_cfg = RAW_OPUS_ENC_CONFIG_DEFAULT();
    opus_cfg.sample_rate        = SAMPLE_RATE;
    opus_cfg.channel            = CHANNEL;
    opus_cfg.bitrate            = BIT_RATE;
    opus_cfg.complexity         = COMPLEXITY;
    encoder_stream = raw_opus_encoder_init(&opus_cfg);
#elif defined (CONFIG_AUDIO_SUPPORT_AAC_DECODER)
    aac_encoder_cfg_t aac_cfg = DEFAULT_AAC_ENCODER_CONFIG();
    aac_cfg.sample_rate        = SAMPLE_RATE;
    aac_cfg.channel            = CHANNEL;
    aac_cfg.bitrate            = BIT_RATE;
    encoder_stream = aac_encoder_init(&aac_cfg);
#elif defined (CONFIG_AUDIO_SUPPORT_G711A_DECODER)
    g711_encoder_cfg_t g711_cfg = DEFAULT_G711_ENCODER_CONFIG();
    encoder_stream = g711_encoder_init(&g711_cfg);
#endif
    return encoder_stream;
}

static void *__audio_record_engine_init(volc_hal_audio_capture_config_t* pipeline, rec_event_cb_t cb)
{
    recorder_sr_cfg_t recorder_sr_cfg = __get_default_audio_record_config();
#if defined (CONFIG_CONTINUOUS_CONVERSATION_MODE)
    recorder_sr_cfg.afe_cfg->wakenet_init = false;
    recorder_sr_cfg.afe_cfg->se_init = false;
    recorder_sr_cfg.afe_cfg->vad_init = false;
#endif // CONFIG_CONTINUOUS_CONVERSATION_MODE

    recorder_encoder_cfg_t recorder_encoder_cfg = { 0 };

#if defined (CONFIG_AUDIO_SUPPORT_G711A_DECODER)
    rsp_filter_cfg_t filter_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    filter_cfg.src_ch = 1;
    filter_cfg.src_rate = 16000;
    filter_cfg.dest_ch = 1;
    filter_cfg.dest_rate = 8000;
    filter_cfg.complexity = 0;
    filter_cfg.stack_in_ext = true;
    filter_cfg.max_indata_bytes = 1024;
    recorder_encoder_cfg.resample = rsp_filter_init(&filter_cfg);
#endif // CONFIG_AUDIO_SUPPORT_G711A_DECODER

    recorder_encoder_cfg.encoder = __create_wake_encoder_stream();

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&__input_cb_for_afe;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.event_cb = cb;
    cfg.vad_off = 1000;
    cfg.wakeup_end = 120000;

#if defined (CONFIG_CONTINUOUS_CONVERSATION_MODE)
    cfg.vad_start = 0;
#endif // CONFIG_CONTINUOUS_CONVERSATION_MODE
    cfg.encoder_handle = recorder_encoder_create(&recorder_encoder_cfg, &cfg.encoder_iface);
    pipeline->recorder_encoder = cfg.encoder_handle;
    pipeline->recorder_engine = audio_recorder_create(&cfg);
#if defined (CONFIG_CONTINUOUS_CONVERSATION_MODE)
    vTaskDelay(pdMS_TO_TICKS(200));
    audio_recorder_trigger_start(pipeline->recorder_engine);
#endif // CONFIG_CONTINUOUS_CONVERSATION_MODE
    return pipeline->recorder_engine;
}

static void __audio_record_engine_deinit(volc_hal_audio_capture_config_t* pipeline)
{
    audio_recorder_destroy(pipeline->recorder_engine);
    recorder_encoder_destroy(pipeline->recorder_encoder);
}

volc_hal_capture_t volc_hal_capture_create(volc_hal_capture_config_t* config)
{
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    if(g_hal_context == NULL){
        return NULL;
    }
    volc_hal_capture_impl_t *capture = (volc_hal_capture_impl_t *)volc_osal_calloc(1, sizeof(volc_hal_capture_impl_t));

    // config
    capture->is_started = false;
    capture->media_type = config->media_type;
    capture->data_cb = config->data_cb;
    capture->audio_wakeup_cb = config->audio_wakeup_cb;
    capture->user_data = config->user_data;
    switch (capture->media_type) {
        case VOLC_MEDIA_TYPE_AUDIO:
            // create and register streams
            audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
            capture->audio_capture_config.audio_pipeline = audio_pipeline_init(&pipeline_cfg);
            mem_assert(capture->audio_capture_config.audio_pipeline);
            // capture->audio_capture_config.i2s_stream_reader = __create_record_i2s_stream();
            // audio_pipeline_register(capture->audio_capture_config.audio_pipeline, capture->audio_capture_config.i2s_stream_reader, "i2s");
            capture->audio_capture_config.algo_aec = __create_record_algo_stream();
            audio_pipeline_register(capture->audio_capture_config.audio_pipeline, capture->audio_capture_config.algo_aec, "algo");
            capture->audio_capture_config.rsp = __create_resample_stream(I2S_SAMPLE_RATE, 1, CODEC_SAMPLE_RATE, 1);
            audio_pipeline_register(capture->audio_capture_config.audio_pipeline, capture->audio_capture_config.rsp, "rsp");
            capture->audio_capture_config.audio_encoder = __create_record_encoder_stream();
            if (capture->audio_capture_config.audio_encoder) {
                audio_pipeline_register(capture->audio_capture_config.audio_pipeline, capture->audio_capture_config.audio_encoder, CODEC_NAME);
            }
            capture->audio_capture_config.raw_reader = __create_record_raw_stream();
            audio_pipeline_register(capture->audio_capture_config.audio_pipeline, capture->audio_capture_config.raw_reader, "raw");
            capture->audio_capture_config.state = INITED;
            g_hal_context->capture_handle[VOLC_HAL_CAPTURE_AUDIO] = (volc_hal_capture_t)capture;
            break;
        case VOLC_MEDIA_TYPE_VIDEO:
            // TODO: Add video capture support
            LOGE("Video capture is not supported yet");
            break;
        default:
            break;
    }
    
    return (volc_hal_capture_t)capture;
}

static void __start_audio_capture(volc_hal_capture_impl_t *impl)
{
    audio_pipeline_unlink(impl->audio_capture_config.audio_pipeline);
    if (impl->audio_capture_config.i2s_stream_reader) {
        audio_pipeline_unregister(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.i2s_stream_reader);
        audio_element_deinit(impl->audio_capture_config.i2s_stream_reader);
    }
    impl->audio_capture_config.i2s_stream_reader = __create_record_i2s_stream(false);
    audio_pipeline_register(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.i2s_stream_reader, "i2s");
#if (CONFIG_VOLC_AUDIO_G711A)
    const char *link_tag[] = {"i2s", "algo", "rsp", CODEC_NAME, "raw"};
#else
    const char *link_tag[] = {"i2s", "algo", "raw"};
#endif
    audio_pipeline_link(impl->audio_capture_config.audio_pipeline, &link_tag[0], sizeof(link_tag) / sizeof(link_tag[0]));
    audio_pipeline_run(impl->audio_capture_config.audio_pipeline);
    // create thread
    volc_osal_thread_param_t param = {0};
    snprintf(param.name, sizeof(param.name), "%s", "volc_capture_task");
    param.stack_size = 4 * 1024;
    param.priority = 5;
    esp_err_t ret = volc_osal_thread_create(&impl->capture_thread, &param, __volc_audio_capture_task, impl);
    mem_assert(ret == 0);
    impl->is_started = true;
    impl->audio_capture_config.state = CAPTURE;
    LOGI("capture task start... mode: CAPTURE");
    return;
}

static void __start_audio_wake(volc_hal_capture_impl_t *impl)
{
    audio_pipeline_unlink(impl->audio_capture_config.audio_pipeline);
    if (impl->audio_capture_config.i2s_stream_reader) {
        audio_pipeline_unregister(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.i2s_stream_reader);
        audio_element_deinit(impl->audio_capture_config.i2s_stream_reader);
    }
    impl->audio_capture_config.i2s_stream_reader = __create_record_i2s_stream(true);
    audio_pipeline_register(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.i2s_stream_reader, "i2s");
    const char *link_tag[] = {"i2s", "raw"};
    audio_pipeline_link(impl->audio_capture_config.audio_pipeline, &link_tag[0], sizeof(link_tag) / sizeof(link_tag[0]));
    audio_pipeline_run(impl->audio_capture_config.audio_pipeline);
    if(impl->audio_wakeup_cb){
        __audio_record_engine_init(&impl->audio_capture_config,(rec_event_cb_t)impl->audio_wakeup_cb);
    }
    impl->is_started = true;
    impl->audio_capture_config.state = WAKEUP;
    LOGI("capture task start... mode: WAKEUP");
    return;
}

int volc_hal_capture_start(volc_hal_capture_t capture,volc_hal_audio_capture_mode_e mode)
{
    volc_hal_capture_impl_t *impl = (volc_hal_capture_impl_t *)capture;
    if (impl->media_type == VOLC_MEDIA_TYPE_AUDIO && impl->audio_capture_config.state == INITED) {
        switch (mode)
        {
        case VOLC_AUDIO_MODE_WAKEUP:
            __start_audio_wake(impl);
            break;
        case VOLC_AUDIO_MODE_CAPTURE:
            __start_audio_capture(impl);
            break;
        default:
            break;
        }
    }
    return 0;
}

int volc_hal_capture_stop(volc_hal_capture_t capture)
{

    volc_hal_capture_impl_t *impl = (volc_hal_capture_impl_t *)capture;

    if (!impl || !impl->is_started) {
        return 0;
    }
    // stop thread
    impl->is_started = false;
    switch (impl->media_type) {
        case VOLC_MEDIA_TYPE_AUDIO:
            if(impl->audio_capture_config.state == WAKEUP){
                __audio_record_engine_deinit(&impl->audio_capture_config);
            }
            audio_pipeline_stop(impl->audio_capture_config.audio_pipeline);
            audio_pipeline_wait_for_stop(impl->audio_capture_config.audio_pipeline);
            audio_pipeline_terminate(impl->audio_capture_config.audio_pipeline);
            raw_stream_write(impl->audio_capture_config.raw_reader, NULL, 0);
            LOGI("capture task stop... mode: %d",impl->audio_capture_config.state);
            impl->audio_capture_config.state = INITED;
            break;
        case VOLC_MEDIA_TYPE_VIDEO:
            // TODO: Add video capture support
            LOGE("Video capture is not supported yet");
            break;
        default:
            break;
    }
    return 0;
}

void volc_hal_capture_destroy(volc_hal_capture_t capture)
{
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    if(g_hal_context == NULL){
        return;
    }

    volc_hal_capture_impl_t *impl = (volc_hal_capture_impl_t *)capture;
    if (!impl) {
        return;
    }

    volc_hal_capture_stop(capture);
    switch (impl->media_type) {
        case VOLC_MEDIA_TYPE_AUDIO:
            if (impl->audio_capture_config.i2s_stream_reader)
            {
                audio_pipeline_unregister(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.i2s_stream_reader);
                audio_element_deinit(impl->audio_capture_config.i2s_stream_reader);
            }
            if (impl->audio_capture_config.audio_encoder) {
                audio_pipeline_unregister(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.audio_encoder);
                audio_element_deinit(impl->audio_capture_config.audio_encoder);
            }
            if (impl->audio_capture_config.raw_reader)
            {
                audio_pipeline_unregister(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.raw_reader);
                audio_element_deinit(impl->audio_capture_config.raw_reader);
            }
            if (impl->audio_capture_config.rsp)
            {
                audio_pipeline_unregister(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.rsp);
                audio_element_deinit(impl->audio_capture_config.rsp);
            }
            if (impl->audio_capture_config.algo_aec)
            {
                audio_pipeline_unregister(impl->audio_capture_config.audio_pipeline, impl->audio_capture_config.algo_aec);
                audio_element_deinit(impl->audio_capture_config.algo_aec);
            }
            impl->user_data = NULL;
            if (impl->audio_capture_config.audio_pipeline) {
                audio_pipeline_deinit(impl->audio_capture_config.audio_pipeline);
                impl->audio_capture_config.audio_pipeline = NULL;
            }
            g_hal_context->capture_handle[VOLC_HAL_CAPTURE_AUDIO] = NULL;
            break;
        case VOLC_MEDIA_TYPE_VIDEO:
            // TODO: Add video capture support
            LOGE("Video capture is not supported yet");
            break;
        default:
            break;
    }
    volc_osal_free(impl);
}
