#include "volc_conv_ai.h"
#include "audio_player.h"
#include "audio_capture.h"
#include "volc_osal.h"
#include "iot_local_function_list.h"
#include <stdio.h>
#include <string.h>
#include "conv_ai.h"


#include "common_def.h"


#include "cJSON.h"
#include "iot_display.h"
#include "audio_vol.h"
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
      \"{\\\"audio\\\":{\\\"codec\\\":{\\\"internal\\\":{\\\"enable\\\":1}}}}\",\
      \"{\\\"rtc\\\":{\\\"access\\\":{\\\"concurrent_requests\\\":1}}}\",\
      \"{\\\"rtc\\\":{\\\"ice\\\":{\\\"concurrent_agents\\\":1}}}\"\
    ]\
  }\
}"

typedef struct
{
    audio_player_handle player_pipeline;
    volc_event_handler_t volc_event_handler;
    volc_engine_t engine;
} engine_context_t;

static char config_buf[1024] = {0};
static engine_context_t engine_ctx = {0};
static bool is_ready = false;

static volatile bool is_interrupt = false;

void conv_ai_task_stop() {
    is_interrupt =  true;
}


static void _on_volc_event(volc_engine_t handle, volc_event_t *event, void *user_data)
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

static void _on_volc_conversation_status(volc_engine_t handle, volc_conv_status_e status, void *user_data)
{
    printf("conversation status changed: %d\n", status);
}

int audio_cnt = 0;
static void _on_volc_audio_data(volc_engine_t handle, const void *data_ptr, size_t data_len, volc_audio_frame_info_t *info_ptr, void *user_data)
{
    int error = 0;
    engine_context_t *demo = (engine_context_t *)user_data;
    if (demo == NULL)
    {
        printf("demo is NULL\n");
        return;
    }
    if (demo->player_pipeline == NULL)
    {
        printf("player pipeline is NULL\n");
        return;
    }
    play_audio(demo->player_pipeline, data_ptr, data_len);
}

static void _on_volc_video_data(volc_engine_t handle, const void *data_ptr, size_t data_len, volc_video_frame_info_t *info_ptr, void *user_data)
{
}


static int sub_offset = 0;
static void on_subtitle_message_received(const cJSON* root) {
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
                iot_display_string(sub + sub_offset);
                sub_offset = 0;
            } else if(strlen(sub) - sub_offset > 18){
                sub_offset = sub_offset == 0 ? 0 : sub_offset -1;
                iot_display_string(sub + sub_offset);
                if(strlen(sub) - sub_offset > 36){
                    sub_offset = strlen(sub);
                }
            }
            // printf("%s %d\n",sub,strlen(sub));
                // printf("subtitle:%s:%s \n", cJSON_GetStringValue(user_id_obj), cJSON_GetStringValue(text_obj));
        }
    }
}

static void on_function_calling_message_received(const cJSON* root, const char* json_str) {
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
                if(strcmp(action,"up") == 0){
                    up_audio_val();
                } else if (strcmp(action,"down") == 0) {
                    down_audio_val();
                }
                volc_send_text_to_agent(engine_ctx.engine,"别着急，我来给你调整音量",VOLC_AGENT_TYPE_TTS);
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
                // printf("send message: %s \n", json_string);
                cJSON_Delete(fc_obj);
                volc_message_info_t info = {0};
                info.is_binary = 1;
                volc_send_message(engine_ctx.engine,fc_message_buffer, json_str_len + 8, &info);
                // printf("fc success \n");
            }
            if(strcmp(func_name,"stop_chat") == 0){
                volc_send_text_to_agent(engine_ctx.engine,"好的拜拜",VOLC_AGENT_TYPE_TTS);
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
                // printf("send message: %s \n", json_string);
                cJSON_Delete(fc_obj);
                volc_message_info_t info = {0};
                info.is_binary = 1;
                volc_send_message(engine_ctx.engine,fc_message_buffer, json_str_len + 8, &info);
                aios_event_pub(Event_Ai_Conversation_QUIT,NULL,NULL);
            }
        }    
    }
}


static void _on_volc_message_data(volc_engine_t handle, const void *message, size_t size, volc_message_info_t *info_ptr, void *user_data)
{
    static char message_buffer[4096];
     if (size > 8) {
        memcpy(message_buffer, message, size);
        message_buffer[size] = 0;
        message_buffer[size + 1] = 0;
        cJSON *root = cJSON_Parse(message_buffer + 8);
        if (root != NULL) {
            if (message_buffer[0] == 's' && message_buffer[1] == 'u' && message_buffer[2] == 'b' && message_buffer[3] == 'v') {
                on_subtitle_message_received(root);
                
            }else if (message_buffer[0] == 't' && message_buffer[1] == 'o' && message_buffer[2] == 'o' && message_buffer[3] == 'l') {
                // function calling 消息
                on_function_calling_message_received(root, message_buffer + 8);
            }
        }
     }
    // printf("Received message: %.*s", (int)size, (const char *)message);
}

volc_event_handler_t volc_event_handler = {0};

void conv_ai_init(){
     snprintf(config_buf, sizeof(config_buf), CONV_AI_CONFIG_FORMAT,
             CONFIG_VOLC_INSTANCE_ID,
             CONFIG_VOLC_PRODUCT_KEY,
             CONFIG_VOLC_PRODUCT_SECRET,
             CONFIG_VOLC_DEVICE_NAME);

    // volc_event_handler = {
    volc_event_handler.on_volc_event = _on_volc_event;
    volc_event_handler.on_volc_conversation_status = _on_volc_conversation_status;
    volc_event_handler.on_volc_audio_data = _on_volc_audio_data;
    volc_event_handler.on_volc_video_data = _on_volc_video_data;
    volc_event_handler.on_volc_message_data = _on_volc_message_data;
    // };
    engine_ctx.volc_event_handler = volc_event_handler;
    int error = volc_create(&engine_ctx.engine, config_buf, &engine_ctx.volc_event_handler, &engine_ctx);
    if (error != 0)
    {
        printf("Failed to create volc engine! error=%d", error);
        return;
    }
}

void conv_ai_task(void *pvParameters)
{
    int error = 0;
    iot_display_string("ai对话创建中");

    // step 1: start audio capture & play
    audio_capture_handle pipeline = audio_capture_create(1,16000);
    audio_player_handle player_pipeline = audio_player_create(1,16000);
    audio_capture_init(pipeline);
    audio_player_init(player_pipeline);
    
    engine_ctx.player_pipeline = player_pipeline;

    // step 2: create ai agent
    // snprintf(config_buf, sizeof(config_buf), CONV_AI_CONFIG_FORMAT,
    //          CONFIG_VOLC_INSTANCE_ID,
    //          CONFIG_VOLC_PRODUCT_KEY,
    //          CONFIG_VOLC_PRODUCT_SECRET,
    //          CONFIG_VOLC_DEVICE_NAME);

    // volc_event_handler_t volc_event_handler = {
    //     .on_volc_event = _on_volc_event,
    //     .on_volc_conversation_status = _on_volc_conversation_status,
    //     .on_volc_audio_data = _on_volc_audio_data,
    //     .on_volc_video_data = _on_volc_video_data,
    //     .on_volc_message_data = _on_volc_message_data,
    // };
   
    // error = volc_create(&engine_ctx.engine, config_buf, &engine_ctx.volc_event_handler, &engine_ctx);
    // if (error != 0)
    // {
    //     printf("Failed to create volc engine! error=%d", error);
    //     return;
    // }
    iot_display_string("ai对话连接中");

    // step 3: start ai agent
    volc_opt_t opt = {
        .mode = VOLC_MODE_RTC,
        .bot_id = CONFIG_VOLC_BOT_ID};
 
    error = volc_start(engine_ctx.engine, &opt);
    if (error != 0)
    {
        printf("Failed to start volc engine! error=%d \n", error);
        volc_destroy(engine_ctx.engine);
        return;
    }
    // int read_size = recorder_pipeline_get_default_read_size(pipeline);
    int read_size = 320;
    uint8_t *audio_buffer = volc_osal_malloc(read_size);
    if (!audio_buffer)
    {
        printf("Failed to alloc audio buffer!");
        return;
    }
    // step 4: start sending audio data
    volc_audio_frame_info_t info = {0};
    info.data_type = VOLC_AUDIO_DATA_TYPE_PCM;
    info.commit = false;
    while (!is_interrupt)
    {   
        int ret = capture_audio(pipeline, (char *)audio_buffer, read_size);
        if (ret == read_size && is_ready)
        {
            // push_audio data
            volc_send_audio_data(engine_ctx.engine, audio_buffer, read_size, &info);
        }
    }
    // step 5: stop audio capture
    audio_capture_destroy(pipeline);
    // step 6: stop and destroy engine
    volc_stop(engine_ctx.engine);
    volc_osal_free(audio_buffer);
    // volc_destroy(engine_ctx.engine);

    // step 7: stop audio play
    audio_player_destroy(player_pipeline);
    // memset(&engine_ctx,0,sizeof(engine_context_t));
    is_ready = false;
    is_interrupt =  false;
    volc_osal_thread_exit(NULL);
}
