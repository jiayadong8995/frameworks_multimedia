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

#include <assert.h>
#include <nuttx/config.h>
#include <nuttx/userspace.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <uv.h>

// include lvgl headers
#include <lvgl/lvgl.h>

#include "ai_common.h"
#include "ai_asr_gui.h"
#include "ai_tool.h"
#include "include/ai_asr.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/
#define SCREEN_WIDTH    (lv_obj_get_width(lv_scr_act()))
#define SCREEN_HEIGHT   (lv_obj_get_height(lv_scr_act()))

#define DEMO_WIDTH (int32_t)((SCREEN_HEIGHT * 0.95f))
#define DEMO_HEIGHT (int32_t)(DEMO_WIDTH)

ai_gui_t g_ai_gui;

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

static void press_timer_cb(lv_timer_t *timer)
{
    return;
}

static void key_press_cb(lv_event_t* e)
{
    lv_obj_t* obj = lv_event_get_target(e);
    if (obj == NULL) {
        return;
    }

}

static void key_release_cb(lv_event_t* e)
{
    lv_obj_t* obj = lv_event_get_target(e);
    if (obj == NULL) {
        return;
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
    
    lv_timer_create(press_timer_cb, 30, NULL);
}

static int ai_asr_engine_init(void* arg)
{
    asr_init_params_t param;
    ai_gui_t* ai_gui = (ai_gui_t*)arg;

    param.loop = &ai_gui->ui_loop;
    param.silence_timeout = 3000; // Set silence timeout to 3 seconds
    ai_gui->asr_ope.handle = ai_asr_create_engine(&param);
    if (ai_gui->asr_ope.handle == NULL) {
        AI_ERR("Failed to create ASR engine\n");
        return -1;
    }

    return 0;
}

static void ai_asr_async_cb(uv_async_queue_t* asyncq, void* data)
{
    asr_thread_t* asr_thread = asyncq->data;
    if (asr_thread->asr_ope.handle_type == ASR_HANDLE_TYPE_STARTED) {
    } else if (asr_thread->asr_ope.handle_type == ASR_HANDLE_TYPE_FINISHED) {
    } else {
        AI_ERR("Unknown asr handle type: %d\n", asr_thread->asr_ope.handle_type);
    }
    return;
}

static void* ai_uvloop_thread(void* arg)
{
    asr_thread_t* asr_thread = (asr_thread_t*)arg;
    uv_loop_t* loop = asr_thread->asrloop;
    int ret;

    ret = uv_loop_init(loop);
    if (ret < 0) {
        AI_ERR("Failed to initialize UV loop: %d\n", ret);
        return NULL;
    }

    asr_thread->asyncq.data = asr_thread;
    ret = uv_async_queue_init(loop, &asr_thread->asyncq, ai_asr_async_cb);

    if (ret < 0) {
        AI_ERR("Failed to initialize UV async queue: %d\n", ret);
        uv_loop_close(loop);
        uv_async_queue_close(&asr_thread->asyncq, NULL);
        return NULL;
    }

    ret = uv_run(asr_thread->asrloop, UV_RUN_DEFAULT);
    if (ret < 0) {
        AI_ERR("Failed to run UV loop: %d\n", ret);
        uv_loop_close(loop);
        uv_async_queue_close(&asr_thread->asyncq, NULL);
        return NULL;
    }

}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int main(int argc, FAR char* argv[])
{
    ai_asr_engine_init(&g_ai_gui);
    
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
