#ifndef IOT_WAKEUP_H__
#define IOT_WAKEUP_H__

typedef int (*iot_wakeup_cb)(void *event, void *user_data);


void iot_wakeup_init(iot_wakeup_cb cb);
void iot_wakeup_deinit();

void iot_wakeup_start();
void iot_wakeup_stop();

void iot_wakeup_register_cb(iot_wakeup_cb cb);

#endif
