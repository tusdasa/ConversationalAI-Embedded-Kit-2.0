/* include ------------------------------------------------------------------ */
#include "aios_ai_conversation_app.h"
#include "conv_ai.h"
#include "iot_wakeup.h"
#include "volc_hal_display.h"
#include "volc_hal_capture.h"


#include "common_def.h"

#include "volc_hal.h"
#include "volc_osal.h"
#include <stdio.h>

/* data structure ----------------------------------------------------------- */
typedef struct aios_Ai_Conversation_app_tag {
    aios_session_t super;
    bool status;
    void* timer;
    volc_osal_tid_t conv_thread_id;
} aios_Ai_Conversation_app_t;

aios_Ai_Conversation_app_t Ai_Conversation_app;

static aios_ret_t state_init(aios_Ai_Conversation_app_t * const me, aios_event_t const * const e);
static aios_ret_t state_conversation(aios_Ai_Conversation_app_t * const me, aios_event_t const * const e);


void aios_Ai_Conversation_app_init(void)
{
    aios_session_init(&Ai_Conversation_app.super, SESSION_PRIORITY_AI_CONVERSATION, NULL);
    aios_session_start(&Ai_Conversation_app.super, AIOS_STATE_CAST(state_init));
}

static aios_ret_t state_init(aios_Ai_Conversation_app_t * const me, aios_event_t const * const e)
{
    AIOS_EVENT_SUB(Event_Ai_Conversation_Start); // 订阅事件
    AIOS_EVENT_SUB(Event_Ai_Conversation_Interrupt); // 订阅事件
    AIOS_EVENT_SUB(Event_Ai_Conversation_QUIT); // 订阅事件
    me->status = 0;
    me->conv_thread_id = NULL;
    conv_ai_init();
    return AIOS_TRAN(state_conversation);
}

static aios_ret_t state_conversation(aios_Ai_Conversation_app_t * const me, aios_event_t const * const e)
{
    volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
    if(g_hal_context == NULL){
        return AIOS_Ret_NotHandled;
    }
    volc_hal_display_t global_display = g_hal_context->display_handle;
    switch ((int)e->id) {
        case Event_Ai_Conversation_Start:
            // printf("state_conversation Event_Ai_Conversation_Start\n");
            if(me->conv_thread_id == NULL){
                volc_osal_thread_param_t param = {0};
                volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"ai对话启动中");
                snprintf(param.name, sizeof(param.name), "%s", "conv_ai");
                param.stack_size = 8*1024;
                param.priority = 5;
                // printf("volc_osal_thread_create  conv_ai_task %d \n",__LINE__);
                volc_osal_thread_create(&me->conv_thread_id,&param,conv_ai_task,NULL);
            }
            return AIOS_Ret_Handled;
        case Event_Ai_Conversation_Interrupt:
            // printf("state_conversation Event_Interrupt\n");
            me->status = 0;
            return AIOS_Ret_Handled;
        case Event_Ai_Conversation_QUIT:
            // printf("state_conversation Event_QUIT\n");
            if(me->conv_thread_id != NULL){
                sleep(5);
                conv_ai_task_stop();
                me->conv_thread_id = NULL;
                // printf("conv_ai_task_stop \n");
            }
            sleep(3);
            volc_hal_capture_start(g_hal_context->capture_handle[VOLC_HAL_CAPTURE_AUDIO],VOLC_AUDIO_MODE_WAKEUP);
            volc_hal_display_set_content(global_display,VOLC_DISPLAY_OBJ_STATUS,VOLC_DISPLAY_TEXT,"请说 hi 乐鑫,启动ai对话");
            return AIOS_Ret_Handled;
            //return AIOS_TRAN(state_on);
        default:
            return AIOS_Ret_NotHandled;
    }
}
