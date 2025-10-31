#ifndef IOT_DISPLAY_H__
#define IOT_DISPLAY_H__

typedef void* img_obj;

void iot_display_init();

void iot_display_string(const char* s);

void iot_display_img(img_obj img);

#endif