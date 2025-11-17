/* include ------------------------------------------------------------------ */
#include "aios_app_manager.h"
#include "aios_ai_conversation_app.h"
#include "iot_wakeup.h"
#include "common_def.h"
#include <stdio.h>
#include "volc_hal.h"
#include "volc_hal_capture.h"


// #include "event_def.h"


/* data structure ----------------------------------------------------------- */
typedef struct aios_app_manager_tag {
    aios_session_t super;
    bool status;
    void* timer;
} aios_app_manager_t;

aios_app_manager_t app_manager;

static aios_ret_t state_init(aios_app_manager_t * const me, aios_event_t const * const e);
static aios_ret_t state_wait_app_event(aios_app_manager_t * const me, aios_event_t const * const e);

/* api ---------------------------------------------------- */
void aios_app_manager_init(void)
{
    aios_session_init(&app_manager.super, SESSION_PRIORITY_APP_MANAGER, NULL);
    aios_session_start(&app_manager.super, AIOS_STATE_CAST(state_init));
}

static aios_ret_t state_init(aios_app_manager_t * const me, aios_event_t const * const e)
{
    AIOS_EVENT_SUB(Event_Network_Match); // 订阅事件
    AIOS_EVENT_SUB(Event_Ai_Conversation); // 订阅事件
    me->status = 0;

    return AIOS_TRAN(state_wait_app_event);
}

static aios_ret_t state_wait_app_event(aios_app_manager_t * const me, aios_event_t const * const e)
{
    switch ((int)e->id) {
        case Event_Network_Match:
            printf("Event_Network_Match\n");
            me->status = 0;
            return AIOS_Ret_Handled;

        case Event_Ai_Conversation:
            printf("Event_Ai_Conversation\n");
             volc_hal_context_t* g_hal_context = volc_get_global_hal_context();
            if(g_hal_context == NULL){
                return AIOS_Ret_NotHandled;
            }
            volc_hal_capture_stop(g_hal_context->capture_handle[VOLC_HAL_CAPTURE_AUDIO]);
            // iot_wakeup_stop();
            // iot_wakeup_deinit();
            aios_event_pub(Event_Ai_Conversation_Start,NULL,NULL);
            printf("Event_Ai_Conversation over\n");

            return AIOS_Ret_Handled;
            // return AIOS_TRAN(state_on);
        default:
            return AIOS_Ret_NotHandled;
    }
}