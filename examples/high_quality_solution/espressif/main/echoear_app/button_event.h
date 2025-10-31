#ifndef AIOS_BUTTON_EVENT_H__
#define AIOS_BUTTON_EVENT_H__

typedef void (* button_cb_t)(void *button_handle, void *usr_data);

int button_init();
int button_register_cb(int button_id, int button_event, button_cb_t cb, void *usr_data);

#endif