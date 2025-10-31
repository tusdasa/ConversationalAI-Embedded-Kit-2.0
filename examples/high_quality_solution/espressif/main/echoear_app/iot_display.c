#include "iot_display.h"

#include "lvgl.h"
#include "bsp/echoear.h"
#include <string.h>

#define LVGL_TASK_PRIORITY       1
#define LVGL_TASK_CORE_ID        1
#define LVGL_TASK_STACK_SIZE     8 * 1024
#define LVGL_TASK_MAX_SLEEP_MS   500
#define LVGL_TASK_TIMER_PERIOD_MS 50
#define LVGL_TASK_STACK_CAPS_EXT  true;

typedef struct {
    lv_obj_t* label;
    lv_timer_t* timer;
    char texts[64];
    uint32_t interval;
    bool is_playing;
} subtitle_manager_t;

static subtitle_manager_t subtitle_mgr = {0};
extern lv_image_dsc_t img_app_pos;
lv_obj_t *g_img = NULL;

 // 获取当前屏幕（根容器）
lv_obj_t *screen = NULL;
extern lv_font_t echoear_font_16;


void subtitle_init() {
    subtitle_mgr.label = lv_label_create(screen);
    lv_obj_set_style_text_font(subtitle_mgr.label, &echoear_font_16, 0);
    // lv_obj_set_size(subtitle_mgr.label, LV_PCT(100), 30); // 宽度100%屏幕，高度50px
    // lv_obj_set_pos(subtitle_mgr.label, 0, 100); // y=50，在顶部区域下方
    lv_obj_align(subtitle_mgr.label, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_height(subtitle_mgr.label, 30);  // 设置固定高度度
    // lv_label_set_long_mode(subtitle_mgr.label, LV_LABEL_LONG_WRAP);  // 自动换行
    // lv_obj_set_style_text_color(subtitle_mgr.label, lv_color_white(), 0);
    // lv_obj_set_style_bg_color(subtitle_mgr.label, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    // lv_obj_set_style_bg_opa(subtitle_mgr.label, LV_OPA_70, 0);
    // lv_obj_set_style_pad_all(subtitle_mgr.label, 8, 0);
    // lv_obj_set_style_radius(subtitle_mgr.label, 5, 0);
}



void subtitle_set_texts(const char* str, uint32_t count) {
    // printf("str %s size %ld \n",str,count);
    memset(subtitle_mgr.texts, 0, 63);
    count = count >= 63 ? 63 : count;
    memcpy(subtitle_mgr.texts,str,count);
    subtitle_mgr.texts[63] = 0;
}

void subtitle_set_interval(uint32_t ms) {
    subtitle_mgr.interval = ms;
}

static void subtitle_update_cb(lv_timer_t* timer) {
    // printf("lv_label_set_text %s \n",subtitle_mgr.texts);
    lv_label_set_text(subtitle_mgr.label, subtitle_mgr.texts);
    lv_obj_remove_flag(subtitle_mgr.label, LV_OBJ_FLAG_HIDDEN);
}

void subtitle_start() {
    if (subtitle_mgr.timer == NULL) {
        subtitle_mgr.timer = lv_timer_create(subtitle_update_cb, 
                                           subtitle_mgr.interval > 0 ? subtitle_mgr.interval : 2000, 
                                           NULL);
        subtitle_mgr.is_playing = true;
        // 立即显示第一段文本
        lv_label_set_text(subtitle_mgr.label, subtitle_mgr.texts);
        lv_obj_remove_flag(subtitle_mgr.label, LV_OBJ_FLAG_HIDDEN);
    }
}

void subtitle_stop() {
    if (subtitle_mgr.timer) {
        lv_timer_del(subtitle_mgr.timer);
        subtitle_mgr.timer = NULL;
    }
    subtitle_mgr.is_playing = false;
}


void iot_display_init(){
    
    // init display
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = {
            .task_priority = LVGL_TASK_PRIORITY,
            .task_stack = LVGL_TASK_STACK_SIZE,
            .task_affinity = LVGL_TASK_CORE_ID,
            .task_max_sleep_ms = LVGL_TASK_MAX_SLEEP_MS,
            .task_stack_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
            .timer_period_ms = LVGL_TASK_TIMER_PERIOD_MS,
        },
        .buffer_size = BSP_LCD_H_RES * 50,
        .double_buffer = true,
        .flags = {
            .buff_spiram = false,
            // .default_dummy_draw = default_dummy_draw, // Avoid white screen during initialization
        },
    };

    lv_disp_t *disp = bsp_display_start_with_config(&cfg);

    // bsp_display_brightness_init();
    bsp_display_backlight_on();
    screen = lv_scr_act();

    subtitle_init();
    subtitle_set_texts("火山demo初始化中",sizeof("火山demo初始化中"));

    subtitle_set_interval(500);  // 0.5秒切换
    subtitle_start();
    iot_display_img(&img_app_pos);
    lv_obj_align(g_img, LV_ALIGN_TOP_MID, 0, 0);
}

void iot_display_string(const char* s){
    // printf("%s\n",s);
    // return;
    subtitle_set_texts(s,strlen(s));
}

void iot_display_img(img_obj img){
    if(g_img == NULL){
        g_img = lv_img_create(lv_scr_act());
    }
    lv_obj_set_size(g_img, 112, 112);
    lv_img_set_src(g_img, img);
    lv_obj_remove_flag(g_img ,LV_OBJ_FLAG_HIDDEN);

}