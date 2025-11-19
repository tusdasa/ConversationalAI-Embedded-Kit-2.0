// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <string.h>

#include "aios.h"
#include "cJSON.h"
#include "volc_hal.h"
#include "volc_osal.h"
#include "util/volc_log.h"
#include "volc_service_common.h"
#include "volc_conv_service.h"
#include "volc_hal.h"
#include "volc_hal_capture.h"
#include "volc_hal_display.h"
#include "volc_hal_player.h"
#include "volc_local_function_list.h"

#if __cplusplus
extern "C" {
#endif

#define CONV_AI_CONFIG_FORMAT "{\
  \"ver\": 1,\
  \"iot\": {\
    \"instance_id\": \"%s\",\
    \"product_key\": \"%s\",\
    \"product_secret\": \"%s\",\
    \"device_name\": \"%s\"\
  },\
  \"rtc\": {\
    \"log_level\": 3,\
    \"audio\": {\
      \"publish\": true,\
      \"subscribe\": true,\
      \"codec\": 3\
    },\
    \"video\": {\
      \"publish\": false,\
      \"subscribe\": false,\
      \"codec\": 1\
    },\
  \"params\":[\
      \"{\\\"debug\\\":{\\\"log_to_console\\\":1}}\",\
      \"{\\\"rtc\\\":{\\\"access\\\":{\\\"concurrent_requests\\\":1}}}\",\
      \"{\\\"rtc\\\":{\\\"ice\\\":{\\\"concurrent_agents\\\":1}}}\",\
      \"{\\\"audio\\\":{\\\"codec\\\":{\\\"pcma\\\":{\\\"s_samples_per_frame\\\":480}}}}\"\
    ]\
  }\
}"

#define CONV_SERVICE_TIMEOUT 15
typedef struct {
    volc_event_handler_t volc_event_handler;
    volc_engine_t engine;
    int wait_time;
} volc_conv_service_t;

static char config_buf[1024] = {0};
static volc_conv_service_t conv_service = {0};
static bool is_ready = false;

static volatile bool is_interrupt = false;
static volc_hal_display_t global_display = NULL;

static void __audio_capture_cb(volc_hal_capture_t capture, const void* data, int len, volc_hal_frame_info_t* frame_info){
    volc_audio_frame_info_t info = {0};
    info.commit = false;
    info.data_type =  frame_info->data_type;
    if(!is_interrupt)
    {
        volc_send_audio_data(conv_service.engine, data, len, &info);
    }
}

static void __on_volc_event(volc_engine_t handle, volc_event_t *event, void *user_data)
{
    switch (event->code)
    {
    case VOLC_EV_CONNECTED:
        is_ready = true;
        printf("Volc Engine connected\n");
        break;
    case VOLC_EV_DISCONNECTED:
        is_ready = false;
        printf( "Volc Engine disconnected\n");
        break;
    default:
        printf("Volc Engine event: %d\n", event->code);
        break;
    }
}

static void __on_volc_conversation_status(volc_engine_t handle, volc_conv_status_e status, void *user_data)
{
    switch (status)
    {
    case VOLC_CONV_STATUS_LISTENING:
        volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"智能体聆听中");
        break;
    case VOLC_CONV_STATUS_THINKING:
        volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"智能体思考中");
        break;
    case VOLC_CONV_STATUS_ANSWERING:
        volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"智能体说话中");
        break;
    case VOLC_CONV_STATUS_INTERRUPTED:
        volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"智能体被打断");
        break;
    case VOLC_CONV_STATUS_ANSWER_FINISH:
        volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"智能体说话完成");
        break;
    default:
        break;
    }
    return;
}

static void __on_volc_audio_data(volc_engine_t handle, const void *data_ptr, size_t data_len, volc_audio_frame_info_t *info_ptr, void *user_data)
{
    int error = 0;
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    if (g_hal_context == NULL) {
        printf("volc_get_global_hal_context failed\n");
        return;
    }

    volc_hal_player_t player_handle = g_hal_context->player_handle[VOLC_HAL_PLAYER_AUDIO];
    
    if (player_handle == NULL)
    {
        LOGE("player pipeline is NULL");
        return;
    }
    volc_hal_player_play_data(player_handle, data_ptr, data_len);
}

static void __on_volc_video_data(volc_engine_t handle, const void *data_ptr, size_t data_len, volc_video_frame_info_t *info_ptr, void *user_data)
{
}

static int sub_offset = 0;
static void __on_subtitle_message_received(const cJSON* root) {
    /*
        {
            "data" : 
            [
                {
                    "definite" : false,
                    "language" : "zh",
                    "mode" : 1,
                    "paragraph" : false,
                    "sequence" : 0,
                    "text" : "\\u4f60\\u597d",
                    "userId" : "voiceChat_xxxxx"
                }
            ],
            "type" : "subtitle"
        }
    */
    cJSON * type_obj = cJSON_GetObjectItem(root, "type");
    if (type_obj != NULL && strcmp("subtitle", cJSON_GetStringValue(type_obj)) == 0) {
        cJSON* data_obj_arr = cJSON_GetObjectItem(root, "data");
        cJSON* obji = NULL;
        cJSON_ArrayForEach(obji, data_obj_arr) {
            cJSON* user_id_obj = cJSON_GetObjectItem(obji, "userId");
            cJSON* text_obj = cJSON_GetObjectItem(obji, "text");
            cJSON* definite_obj = cJSON_GetObjectItem(obji, "definite");
            char* sub = cJSON_GetStringValue(text_obj);

            // print_string(sub + sub_offset -1);
            if(cJSON_IsTrue(definite_obj)){
                sub_offset = sub_offset == 0 ? 0 : sub_offset -1;
                volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_SUBTITLE,VOLC_DISPLAY_TEXT,sub + sub_offset);
                sub_offset = 0;
            } else if(strlen(sub) - sub_offset > 18){
                sub_offset = sub_offset == 0 ? 0 : sub_offset -1;
                volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_SUBTITLE,VOLC_DISPLAY_TEXT,sub + sub_offset);
                if(strlen(sub) - sub_offset > 36){
                    sub_offset = strlen(sub);
                }
            }
        }
    }
}

static void __on_function_calling_message_received(const cJSON* root, const char* json_str) {
    /*
        {
            "subscriber_user_id" : "",
            "tool_calls" : 
            [
                {
                    "function" : 
                    {
                        "arguments" : "{\\"location\\": \\"\\u5317\\u4eac\\u5e02\\"}",
                        "name" : "get_current_weather"
                    },
                    "id" : "call_py400kek0e3pczrqdxgnb3lo",
                    "type" : "function"
                }
            ]
        }
    */
   
    cJSON* tool_obj_arr = cJSON_GetObjectItem(root, "tool_calls");
    cJSON* obji = NULL;
    cJSON_ArrayForEach(obji, tool_obj_arr){
        cJSON* id_obj = cJSON_GetObjectItem(obji, "id");
        cJSON* function_obj = cJSON_GetObjectItem(obji, "function");
        if (id_obj && function_obj) {
            cJSON* arguments_obj = cJSON_GetObjectItem(function_obj, "arguments");
            cJSON* name_obj = cJSON_GetObjectItem(function_obj, "name");
            char* arguments_json_str = cJSON_GetStringValue(arguments_obj);
            const char* func_name = cJSON_GetStringValue(name_obj);
            const char* func_id = cJSON_GetStringValue(id_obj);
            if (strcmp(func_name, "adjust_audio_val") == 0 && arguments_json_str) {               
                cJSON *arguments_json = cJSON_Parse(arguments_json_str);
                cJSON* action_obj = cJSON_GetObjectItem(arguments_json, "action");
                const char* action = (action_obj->valuestring);
                adjust_audio_val(action);
                volc_send_text_to_agent(conv_service.engine,"别着急，我来给你调整音量",VOLC_AGENT_TYPE_TTS);
                cJSON *fc_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(fc_obj, "ToolCallID", func_id);
                cJSON_AddStringToObject(fc_obj, "Content", "音量已经调整了");
                char *json_string = cJSON_Print(fc_obj);
                static char fc_message_buffer[256] = {'f', 'u', 'n', 'c'};
                int json_str_len = strlen(json_string);
                fc_message_buffer[4] = (json_str_len >> 24) & 0xff;
                fc_message_buffer[5] = (json_str_len >> 16) & 0xff;
                fc_message_buffer[6] = (json_str_len >> 8) & 0xff;
                fc_message_buffer[7] = (json_str_len >> 0) & 0xff;
                memcpy(fc_message_buffer + 8, json_string, json_str_len);
                cJSON_Delete(fc_obj);
                volc_message_info_t info = {0};
                info.is_binary = 1;
                volc_send_message(conv_service.engine,fc_message_buffer, json_str_len + 8, &info);
            }
            if(strcmp(func_name,"stop_chat") == 0){
                volc_send_text_to_agent(conv_service.engine,"好的拜拜",VOLC_AGENT_TYPE_TTS);
                cJSON *fc_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(fc_obj, "ToolCallID", func_id);
                cJSON_AddStringToObject(fc_obj, "Content", "对话即将结束");
                char *json_string = cJSON_Print(fc_obj);
                static char fc_message_buffer[256] = {'f', 'u', 'n', 'c'};
                int json_str_len = strlen(json_string);
                fc_message_buffer[4] = (json_str_len >> 24) & 0xff;
                fc_message_buffer[5] = (json_str_len >> 16) & 0xff;
                fc_message_buffer[6] = (json_str_len >> 8) & 0xff;
                fc_message_buffer[7] = (json_str_len >> 0) & 0xff;
                memcpy(fc_message_buffer + 8, json_string, json_str_len);
                cJSON_Delete(fc_obj);
                volc_message_info_t info = {0};
                info.is_binary = 1;
                volc_send_message(conv_service.engine, fc_message_buffer, json_str_len + 8, &info);
                aios_event_pub(VOLC_SERVICE_AI_CONVERSATION_QUIT, NULL, NULL);
            }
        }    
    }
}

static void __on_volc_message_data(volc_engine_t handle, const void *message, size_t size, volc_message_info_t *info_ptr, void *user_data)
{
    static char message_buffer[4096];
     if (size > 8) {
        memcpy(message_buffer, message, size);
        message_buffer[size] = 0;
        message_buffer[size + 1] = 0;
        cJSON *root = cJSON_Parse(message_buffer + 8);
        if (root != NULL) {
            if (message_buffer[0] == 's' && message_buffer[1] == 'u' && message_buffer[2] == 'b' && message_buffer[3] == 'v') {
                __on_subtitle_message_received(root);
                conv_service.wait_time = 0;
                
            } else if (message_buffer[0] == 't' && message_buffer[1] == 'o' && message_buffer[2] == 'o' && message_buffer[3] == 'l') {
                // function calling 消息
                __on_function_calling_message_received(root, message_buffer + 8);
            }
        }
     }
}

void conv_ai_service_init()
{
    snprintf(config_buf, sizeof(config_buf), CONV_AI_CONFIG_FORMAT,
             CONFIG_VOLC_INSTANCE_ID,
             CONFIG_VOLC_PRODUCT_KEY,
             CONFIG_VOLC_PRODUCT_SECRET,
             CONFIG_VOLC_DEVICE_NAME);

    conv_service.volc_event_handler = (volc_event_handler_t){
        .on_volc_event = __on_volc_event,
        .on_volc_conversation_status = __on_volc_conversation_status,
        .on_volc_audio_data = __on_volc_audio_data,
        .on_volc_video_data = __on_volc_video_data,
        .on_volc_message_data = __on_volc_message_data,
    };

    int ret = volc_create(&conv_service.engine, config_buf, &conv_service.volc_event_handler, &conv_service);
    if (ret != 0) {
        LOGE("volc_create failed, ret: %d", ret);
        return;
    } else {
        LOGE("volc_create success");
    }
}

void conv_ai_service_task(void *pvParameters)
{
    is_interrupt = false;
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    char* status_str = NULL;
    if (g_hal_context == NULL) {
        LOGE("volc_get_global_hal_context failed");
        status_str = "系统未初始化,ai对话连接失败";
        goto CONV_AI_QUIT;
    }
    global_display = g_hal_context->display_handle;
    int ret = 0;
    volc_hal_display_set_content(global_display, VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT, "ai对话创建中");

    // step 1: start audio capture & play
    volc_hal_capture_config_t  capture_audio_config  = {0};
    capture_audio_config.media_type = VOLC_MEDIA_TYPE_AUDIO;
    capture_audio_config.data_cb = __audio_capture_cb;
    capture_audio_config.user_data = g_hal_context;
    volc_hal_capture_t capture_handle = g_hal_context->capture_handle[VOLC_HAL_PLAYER_AUDIO];
    if (g_hal_context->capture_handle[VOLC_HAL_PLAYER_AUDIO] == NULL) {
        g_hal_context->capture_handle[VOLC_HAL_PLAYER_AUDIO] = volc_hal_capture_create(&capture_audio_config);
    }
    volc_hal_capture_stop(g_hal_context->capture_handle[VOLC_HAL_CAPTURE_AUDIO]);
    volc_hal_capture_update_config(capture_handle, &capture_audio_config);
    volc_hal_capture_start(g_hal_context->capture_handle[VOLC_HAL_PLAYER_AUDIO], VOLC_AUDIO_MODE_CAPTURE);

    volc_hal_player_config_t player_audio_config = {0};
    player_audio_config.media_type = VOLC_MEDIA_TYPE_AUDIO;
    if (g_hal_context->player_handle[VOLC_HAL_PLAYER_AUDIO] == NULL) {
        g_hal_context->player_handle[VOLC_HAL_PLAYER_AUDIO] = volc_hal_player_create(&player_audio_config);
    }
    
    volc_hal_display_set_content(global_display, VOLC_DISPLAY_OBJ_STATUS, VOLC_DISPLAY_TEXT, "ai对话连接中");
    volc_hal_player_start(g_hal_context->player_handle[VOLC_HAL_PLAYER_AUDIO]);

    // step 2: start ai conversation
    volc_opt_t opt = {
        .mode = VOLC_MODE_RTC,
        .bot_id = CONFIG_VOLC_BOT_ID,
    };

    ret = volc_start(conv_service.engine, &opt);
    if (ret != 0) {
        LOGE("volc_start failed, ret: %d", ret);
        status_str = "ai对话连接失败,准备退出";
        goto CONV_AI_QUIT;
    }
    volc_hal_display_set_content(global_display, VOLC_DISPLAY_OBJ_STATUS, VOLC_DISPLAY_TEXT, "ai对话已连接");

    while (!is_interrupt) {
        sleep(1);
        conv_service.wait_time++;
        if(conv_service.wait_time >= CONV_SERVICE_TIMEOUT){
            status_str = "ai对话已超时,准备退出";
            goto CONV_AI_QUIT;
        }
    }
    status_str = "ai对话已断开";

CONV_AI_QUIT:
    aios_event_pub(VOLC_SERVICE_AI_CONVERSATION_OVER, NULL, NULL);
    volc_hal_capture_stop(g_hal_context->capture_handle[VOLC_HAL_PLAYER_AUDIO]);
    volc_hal_player_stop(g_hal_context->player_handle[VOLC_HAL_PLAYER_AUDIO]);
    volc_hal_player_destroy(g_hal_context->player_handle[VOLC_HAL_PLAYER_AUDIO]);

    volc_stop(conv_service.engine);
    if(status_str != NULL){
        volc_hal_display_set_content(global_display, VOLC_DISPLAY_OBJ_STATUS, VOLC_DISPLAY_TEXT, status_str);
    }
    volc_hal_display_set_content(global_display, VOLC_DISPLAY_OBJ_SUBTITLE, VOLC_DISPLAY_TEXT, "");
    // just show the status: Ai对话已断开
    sleep(1);
    is_ready = false;
    volc_hal_capture_start(g_hal_context->capture_handle[VOLC_HAL_CAPTURE_AUDIO],VOLC_AUDIO_MODE_WAKEUP);
    volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"请说 hi 乐鑫,启动ai对话");
    volc_osal_thread_exit(NULL);
}

void conv_ai_service_task_stop(void)
{
    // sleep for AI Agent say goodbye
    sleep(5);
    is_interrupt =  true;
}

#if __cplusplus
}
#endif
