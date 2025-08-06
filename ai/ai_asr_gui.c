/****************************************************************************
 * frameworks/ai/src/asr/ai_asr_gui.c
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
#include "ai_asr_gui.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/
#define SCREEN_WIDTH    (lv_obj_get_width(lv_scr_act()))
#define SCREEN_HEIGHT   (lv_obj_get_height(lv_scr_act()))

#define DEMO_WIDTH (int32_t)((SCREEN_HEIGHT * 0.95f))
#define DEMO_HEIGHT (int32_t)(DEMO_WIDTH)

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static void lv_nuttx_uv_loop(uv_loop_t* loop, lv_nuttx_result_t* result)
{
    lv_nuttx_uv_t uv_info;
    void* data;
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

static void key_press_cb(lv_event_t* e)
{
    ai_gui_t* ai_gui = (ai_gui_t*)lv_event_get_user_data(e);
    if(ai_gui == NULL) {
        return;
    }
    ai_asr_start(ai_gui->handle, NULL);
}

static void key_release_cb(lv_event_t* e)
{
    ai_gui_t* ai_gui = (ai_gui_t*)lv_event_get_user_data(e);
    if(ai_gui == NULL) {
        return;
    }
    ai_asr_finish(ai_gui->handle);
}

static void clear_text_cb(lv_event_t* e)
{
    ai_gui_t* ai_gui = (ai_gui_t*)lv_event_get_user_data(e);
    lv_obj_t* obj = lv_event_get_target(e);
    if (obj == NULL) {
        return;
    }

    lv_textarea_set_text(ai_gui->ui.textarea, "");
}

static void ui_create(ai_gui_t* arg)
{
    if (arg == NULL)
    {
        return;
    }
    ai_gui_t* ai_gui = arg;

    LV_FONT_DECLARE(lv_font_siyuan_16);
    LV_IMG_DECLARE(mic);

    ai_gui->ui.textarea_font = &lv_font_siyuan_16;

    if (ai_gui->ui.textarea_font == NULL) {
        LV_LOG_ERROR("Failed to create text area font\n");
        return;
    }

    // Create main container
    ai_gui->ui.root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(ai_gui->ui.root);
    lv_obj_center(ai_gui->ui.root);
    lv_obj_set_size(ai_gui->ui.root, DEMO_WIDTH, DEMO_HEIGHT);
    lv_obj_set_flex_flow(ai_gui->ui.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ai_gui->ui.root, LV_FLEX_ALIGN_CENTER,\
                     LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(ai_gui->ui.root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ai_gui->ui.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ai_gui->ui.root, 16, 0); // Add padding
    
    // Create title
    ai_gui->ui.title = lv_label_create(ai_gui->ui.root);
    lv_label_set_text(ai_gui->ui.title, "Speech to Text");
    lv_obj_set_style_text_color(ai_gui->ui.title, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_text_font(ai_gui->ui.title, &lv_font_montserrat_32, 0);
    lv_obj_set_size(ai_gui->ui.title, LV_PCT(100), LV_PCT(10));
    lv_obj_set_style_text_align(ai_gui->ui.title, LV_TEXT_ALIGN_CENTER, 0);
    
    // Create text area
    ai_gui->ui.textarea = lv_textarea_create(ai_gui->ui.root);
    lv_obj_set_size(ai_gui->ui.textarea, LV_PCT(100), LV_PCT(50));
    lv_obj_set_style_text_align(ai_gui->ui.textarea, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(ai_gui->ui.textarea, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_border_color(ai_gui->ui.textarea, lv_color_hex(0xBDC3C7),\
                                                            0);
    lv_obj_set_style_border_width(ai_gui->ui.textarea, 1, 0);
    lv_obj_set_style_radius(ai_gui->ui.textarea, 8, 0);
    lv_obj_set_style_bg_color(ai_gui->ui.textarea, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(ai_gui->ui.textarea, ai_gui->ui.textarea_font, 0);
    lv_textarea_set_placeholder_text(ai_gui->ui.textarea,\
                         "Speech recognition results will appear here...");

    // Create voice button
    ai_gui->ui.voice_btntnm = lv_button_create(ai_gui->ui.root);
    lv_obj_set_size(ai_gui->ui.voice_btntnm, LV_PCT(20), LV_PCT(20));
    
    lv_obj_set_style_radius(ai_gui->ui.voice_btntnm, LV_RADIUS_CIRCLE, 0);

    lv_obj_set_style_bg_color(ai_gui->ui.voice_btntnm, lv_color_hex(0x3498DB),\
                                                            0); // Blue background
    lv_obj_set_style_bg_color(ai_gui->ui.voice_btntnm, lv_color_hex(0x2980B9),\
                                                            LV_STATE_PRESSED); 
    
    ai_gui->ui.clear_btnm = lv_btn_create(ai_gui->ui.root);
    lv_obj_set_size(ai_gui->ui.clear_btnm, 40, 40);
    lv_obj_set_style_bg_opa(ai_gui->ui.clear_btnm, LV_OPA_0, 0); 
    lv_obj_set_style_border_opa(ai_gui->ui.clear_btnm, LV_OPA_0, 0);

    ai_gui->ui.clear_btnm_img = lv_label_create(ai_gui->ui.clear_btnm);
    lv_label_set_text(ai_gui->ui.clear_btnm_img, LV_SYMBOL_CLOSE); 
    lv_obj_set_style_text_color(ai_gui->ui.clear_btnm_img,\
                         lv_color_hex(0x666666), 0);
    lv_obj_center(ai_gui->ui.clear_btnm_img);

    // mic imag
    ai_gui->ui.voice_btntnm_img = lv_image_create(ai_gui->ui.voice_btntnm);
    lv_img_set_src(ai_gui->ui.voice_btntnm_img, &mic);
    lv_img_set_zoom(ai_gui->ui.voice_btntnm_img,64);
    lv_obj_center(ai_gui->ui.voice_btntnm_img);

    // Add event callbacks
    lv_obj_add_event_cb(ai_gui->ui.voice_btntnm, key_press_cb, \
                                                LV_EVENT_PRESSED, ai_gui);
    lv_obj_add_event_cb(ai_gui->ui.voice_btntnm, key_release_cb, \
                                                LV_EVENT_RELEASED, ai_gui);
    lv_obj_add_event_cb(ai_gui->ui.clear_btnm, clear_text_cb, \
                                                LV_EVENT_CLICKED, ai_gui);

}

static void asr_gui_callback(asr_event_t event, const asr_result_t* result, void* cookie)
{
    ai_gui_t* ai_gui = (ai_gui_t*)cookie;
    if (event == asr_event_result) {
        if (result && result->result) {
            const char* asr_result = result->result;
            if (strcmp(asr_result, "") == 0) {
                lv_textarea_set_text(ai_gui->ui.textarea, "No result");
            } else {
                lv_textarea_set_text(ai_gui->ui.textarea, asr_result);
            }
        }else {
            lv_textarea_set_text(ai_gui->ui.textarea, "Error occurred");
        }
        LV_LOG_INFO("asr_event_result\n");
    } else if (event == asr_event_complete) {
        printf("asr_event_complete\n");
        LV_LOG_INFO("asr_event_complete\n");
    } else if (event == asr_event_error) {
        if (result) {
            LV_LOG_ERROR("ASR error: %d\n", result->error_code);
            lv_textarea_set_text(ai_gui->ui.textarea, "ASR error occurred");
        }
        LV_LOG_ERROR("asr_event_error\n");
    } else if (event == asr_event_start) {
        LV_LOG_INFO("ASR started\n");
    } else if (event == asr_event_cancel) {
        LV_LOG_INFO("ASR stopped\n");
    } else if (event == asr_event_closed) {
        LV_LOG_INFO("ASR closed\n");
    } else {
        LV_LOG_ERROR("Unknown ASR event: %d\n", event);
    }
}

static int ai_asr_engine_init(ai_gui_t* arg)
{
    asr_init_params_t param;
    ai_gui_t* ai_gui = arg;

    param.loop = &ai_gui->ui_loop;
    param.silence_timeout = 3000; // Set silence timeout to 3 seconds
    ai_gui->handle = ai_asr_create_engine(&param);
    if (ai_gui->handle == NULL) {
        LV_LOG_ERROR("Failed to create ASR engine\n");
        return -1;
    }

    ai_asr_set_listener(ai_gui->handle ,\
                        asr_gui_callback , ai_gui);

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int main(int argc, FAR char* argv[])
{
    ai_gui_t ai_gui;
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

    uv_loop_init(&ai_gui.ui_loop);

    ui_create(&ai_gui);
    ai_asr_engine_init(&ai_gui);
    // refresh lvgl ui
    lv_nuttx_uv_loop(&ai_gui.ui_loop, &result);

    lv_nuttx_deinit(&result);
    lv_deinit();

    return 0;
}
