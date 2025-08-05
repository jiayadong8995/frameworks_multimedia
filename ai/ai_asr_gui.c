/****************************************************************************
 * frameworks/ai/src/asr/ai_lvgl.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/userspace.h>
#include <pthread.h>
#include <unistd.h>
#include <uv.h>

// include lvgl headers
#include <lvgl/lvgl.h>

#include "ai_common.h"
#include "ai_asr_gui.h"
#include "ai_tool.h"
/****************************************************************************
 * Private Types
 ****************************************************************************/
ai_gui_t g_ai_gui;

static lv_timer_t *press_timer = NULL; // Define the timer pointer

extern const lv_image_dsc_t mic;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static void lv_nuttx_uv_loop(uv_loop_t* loop, lv_nuttx_result_t* result)
{
    lv_nuttx_uv_t uv_info;
    void* data;

    uv_loop_init(loop);

    lv_memset(&uv_info, 0, sizeof(uv_info));
    uv_info.loop = loop;
    uv_info.disp = result->disp;
    uv_info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
    uv_info.uindev = result->utouch_indev;
#endif

    data = lv_nuttx_uv_init(&uv_info);
    uv_run(loop, UV_RUN_DEFAULT);
    lv_nuttx_uv_deinit(&data);
}

// Timer callback function that is executed repeatedly during key presses
static void press_timer_cb(lv_timer_t *timer)
{
    // pthread_mutex_lock(&asr_result_mutex);
    // const char* asr_result_text = global_asr_result.result;
    // pthread_mutex_unlock(&asr_result_mutex);

    // if (asr_result_text != NULL) {
    //     AI_INFO("ASR result text: %s, length: %zu\n", asr_result_text, strlen(asr_result_text));
    //     fflush(stdout);
    //     lv_textarea_set_text(g_ai_gui.ui.textarea, asr_result_text);
    //     AI_INFO("lv_textarea_set_text\n");
    // } else {
    //     lv_textarea_set_text(g_ai_gui.ui.textarea, "No ASR result available");
    // }

    // aitool_cmd_start_exec(&aitool, 0);
}

static void key_press_cb(lv_event_t* e)
{
    lv_obj_t* obj = lv_event_get_target(e);
    if (obj == NULL) {
        return;
    }

    if (press_timer == NULL) {
        press_timer = lv_timer_create(press_timer_cb, 30, NULL);
    }
}

static void key_release_cb(lv_event_t* e)
{
    lv_obj_t* obj = lv_event_get_target(e);
    if (obj == NULL) {
        return;
    }

    // Locking to protect global variables
    // pthread_mutex_lock(&asr_result_mutex);
    // const char* asr_result_text = global_asr_result.result;
    // pthread_mutex_unlock(&asr_result_mutex);

    // if(NULL == asr_result_text){
    //     asr_result_text = "No ASR result available";
    // }

    // lv_textarea_set_text(g_ai_gui.ui.textarea, asr_result_text);
    // aitool_cmd_finish_exec(&aitool, 0);

    if(press_timer != NULL){
        lv_timer_del(press_timer);
        press_timer = NULL;
    }
}

static void clear_text_cb(lv_event_t* e)
{
    lv_obj_t* obj = lv_event_get_target(e);
    if (obj == NULL) {
        return;
    }

    lv_textarea_set_text(g_ai_gui.ui.textarea, "");
}

static void ui_create(void)
{
    // g_ai_gui.ui.textarea_font = lv_freetype_font_create("/data/res/fonts/MiSans-Normal.ttf",\
    //                      LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 24, LV_FREETYPE_FONT_STYLE_NORMAL);
    
    LV_FONT_DECLARE(lv_font_siyuan_16);

    g_ai_gui.ui.textarea_font = &lv_font_siyuan_16;

    if (g_ai_gui.ui.textarea_font == NULL) {
        AI_ERR("Failed to create text area font\n");
        return;
    }

    // Create main container
    g_ai_gui.ui.root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(g_ai_gui.ui.root);
    lv_obj_center(g_ai_gui.ui.root);
    lv_obj_set_size(g_ai_gui.ui.root, DEMO_WIDTH, DEMO_HEIGHT);
    lv_obj_set_flex_flow(g_ai_gui.ui.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_ai_gui.ui.root, LV_FLEX_ALIGN_CENTER,\
                     LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(g_ai_gui.ui.root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_ai_gui.ui.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_ai_gui.ui.root, 16, 0); // Add padding
    
    // Create title
    g_ai_gui.ui.title = lv_label_create(g_ai_gui.ui.root);
    lv_label_set_text(g_ai_gui.ui.title, "Speech to Text");
    lv_obj_set_style_text_color(g_ai_gui.ui.title, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_text_font(g_ai_gui.ui.title, &lv_font_montserrat_32, 0);
    lv_obj_set_size(g_ai_gui.ui.title, LV_PCT(100), LV_PCT(10));
    lv_obj_set_style_text_align(g_ai_gui.ui.title, LV_TEXT_ALIGN_CENTER, 0);
    
    // Create text area
    g_ai_gui.ui.textarea = lv_textarea_create(g_ai_gui.ui.root);
    lv_obj_set_size(g_ai_gui.ui.textarea, LV_PCT(100), LV_PCT(50));
    lv_obj_set_style_text_align(g_ai_gui.ui.textarea, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(g_ai_gui.ui.textarea, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_border_color(g_ai_gui.ui.textarea, lv_color_hex(0xBDC3C7), 0);
    lv_obj_set_style_border_width(g_ai_gui.ui.textarea, 1, 0);
    lv_obj_set_style_radius(g_ai_gui.ui.textarea, 8, 0);
    lv_obj_set_style_bg_color(g_ai_gui.ui.textarea, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(g_ai_gui.ui.textarea, g_ai_gui.ui.textarea_font, 0);
    lv_textarea_set_placeholder_text(g_ai_gui.ui.textarea,\
                         "Speech recognition results will appear here...");

    // Create voice button
    g_ai_gui.ui.voice_btntnm = lv_button_create(g_ai_gui.ui.root);
    lv_obj_set_size(g_ai_gui.ui.voice_btntnm, LV_PCT(20), LV_PCT(20));
    
    lv_obj_set_style_radius(g_ai_gui.ui.voice_btntnm, LV_RADIUS_CIRCLE, 0); // Circular button

    lv_obj_set_style_bg_color(g_ai_gui.ui.voice_btntnm, lv_color_hex(0x3498DB),\
                                                            0); // Blue background
    lv_obj_set_style_bg_color(g_ai_gui.ui.voice_btntnm, lv_color_hex(0x2980B9),\
                                                            LV_STATE_PRESSED); // Pressed state color
    
    g_ai_gui.ui.clear_btnm = lv_btn_create(g_ai_gui.ui.root);
    lv_obj_set_size(g_ai_gui.ui.clear_btnm, 40, 40);
    lv_obj_set_style_bg_opa(g_ai_gui.ui.clear_btnm, LV_OPA_0, 0); 
    lv_obj_set_style_border_opa(g_ai_gui.ui.clear_btnm, LV_OPA_0, 0);

    g_ai_gui.ui.clear_btnm_img = lv_label_create(g_ai_gui.ui.clear_btnm);
    lv_label_set_text(g_ai_gui.ui.clear_btnm_img, LV_SYMBOL_CLOSE); 
    lv_obj_set_style_text_color(g_ai_gui.ui.clear_btnm_img, lv_color_hex(0x666666), 0);
    lv_obj_center(g_ai_gui.ui.clear_btnm_img);

    // mic imag
    g_ai_gui.ui.voice_btntnm_img = lv_image_create(g_ai_gui.ui.voice_btntnm);
    lv_img_set_src(g_ai_gui.ui.voice_btntnm_img, &mic);
    lv_img_set_zoom(g_ai_gui.ui.voice_btntnm_img,64);
    lv_obj_center(g_ai_gui.ui.voice_btntnm_img);

    // Add event callbacks
    lv_obj_add_event_cb(g_ai_gui.ui.voice_btntnm, key_press_cb, \
                                                LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_ai_gui.ui.voice_btntnm, key_release_cb, \
                                                LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_ai_gui.ui.clear_btnm, clear_text_cb, \
                                                LV_EVENT_CLICKED, NULL); 
}

static void* ai_uvloop_thread(void* arg)
{

    return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int main(int argc, FAR char* argv[])
{
    asr_thread_t asr_thread;
    memset(&asr_thread, 0, sizeof(asr_thread_t));
    pthread_attr_init(asr_thread.attr);
    pthread_attr_setdetachstate(asr_thread.attr, 16384);
    pthread_create(asr_thread.ai_uvloop_tid, asr_thread.attr,\
                    ai_uvloop_thread, NULL);

    // init lvgl
    lv_nuttx_dsc_t info;
    lv_nuttx_result_t result;

    if (lv_is_initialized()) {
        LV_LOG_ERROR("LVGL already initialized! aborting.");
        return -1;
    }

    lv_init();

    lv_nuttx_dsc_init(&info);
    lv_nuttx_init(&info, &result);

    if (result.disp == NULL) {
        LV_LOG_ERROR("lv_demos initialization failure!");
        return -2;
    }

    ui_create();

    // refresh lvgl ui
    lv_nuttx_uv_loop(&g_ai_gui.ui_loop, &result);

    lv_nuttx_deinit(&result);
    lv_deinit();

    return 0;
}
