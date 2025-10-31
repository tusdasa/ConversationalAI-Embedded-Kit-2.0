#ifndef AIOS_COMMON_DEF_H__
#define AIOS_COMMON_DEF_H__
#include "aios.h"

enum {
    Event_Test = Event_User,
    Event_Network_Match,
    Event_Ai_Conversation,
    
    // Ai_Conversation event:
    Event_Ai_Conversation_Start = 100,
    Event_Ai_Conversation_Interrupt = 101,
    Event_Ai_Conversation_QUIT = 102,
    Event_Max
};

enum {
    SESSION_PRIORITY_AI_CONVERSATION = 0,
    SESSION_PRIORITY_APP_MANAGER,
};

#endif