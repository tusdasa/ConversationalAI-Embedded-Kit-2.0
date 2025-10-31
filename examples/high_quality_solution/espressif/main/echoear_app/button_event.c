#include "button_event.h"
#include "iot_button.h"
#include "button_gpio.h"
//    char* name[6] = {"vol+","vol-","set","play","mute","rec"};
/*
vol+: min 190 max 600 
vol-: min 600 max 1000 
set: min 1000 max 1375 
play: min 1375 max 1775 
mute: min 1775 max 2195 
rec: min 2195 max 2705 
*/

// button_adc_config_t esp32_button_config[6];
 button_config_t esp32_button_config[6];
const uint16_t vol[6] = {380, 820, 1180, 1570, 1980, 2410};
char* name[6] = {"vol+","vol-","set","play","mute","rec"};

enum {
    BUTTON_VOL_ADD,
    BUTTON_VOL_SUB,
    BUTTON_SET,
    BUTTON_PLAY,
    BUTTON_MUTE,
    BUTTON_REC
};

int button_init(){
        for (size_t i = 0; i < 6; i++) {
        esp32_button_config[i].type = BUTTON_TYPE_ADC;
        esp32_button_config[i].adc_button_config.adc_channel = 4;
        esp32_button_config[i].adc_button_config.button_index = i;

        if (i == 0) {
            esp32_button_config[i].adc_button_config.min = (0 + vol[i]) / 2;
        } else {
            esp32_button_config[i].adc_button_config.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == 5) {
            esp32_button_config[i].adc_button_config.max = (vol[i] + 3000) / 2;
        } else {
            esp32_button_config[i].adc_button_config.max = (vol[i] + vol[i + 1]) / 2;
        }
    }
    return 0;
}

int button_register_cb(int button_id, int button_event, button_cb_t cb, void *usr_data){
    button_handle_t btn = NULL;
    btn = iot_button_create(esp32_button_config + button_id);
    iot_button_register_cb(btn, button_event, cb, usr_data);
    // iot_button_register_cb(btn, BUTTON_PRESS_UP, cb, usr_data);
    return 0;
}
