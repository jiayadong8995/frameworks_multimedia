/****************************************************************************
 * frameworks/ai/src/asr/ai_conversation_gui.c
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

#include <ai_conversation.h>
#include <ai_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lvgl.h>

/****************************************************************************
 * Private Types
 ****************************************************************************/
#define SCREEN_WIDTH    (lv_obj_get_width(lv_scr_act()))
#define SCREEN_HEIGHT   (lv_obj_get_height(lv_scr_act()))

#define DEMO_WIDTH (int32_t)((SCREEN_HEIGHT * 0.95f))
#define DEMO_HEIGHT (int32_t)(DEMO_WIDTH)

typedef struct conver_gui_s
{
    uv_loop_t ui_loop;
    void* handle;
    bool conversation_active;
    struct
    {
        lv_obj_t *root;
        lv_obj_t *title;
        lv_obj_t *status_bar;
        lv_obj_t *status_label;
        lv_obj_t *clear_btnm;
        lv_obj_t *clear_btnm_img;
        lv_obj_t *voice_btntnm;
        lv_obj_t *voice_btntnm_img;
        lv_obj_t *textarea;
        const lv_font_t *textarea_font;
    }ui;

}conver_gui_t;

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
    int ret = 0;
    conver_gui_t* ai_gui = (conver_gui_t*)lv_event_get_user_data(e);
    if(ai_gui == NULL) {
        return;
    }

    if (ai_gui->conversation_active) {
        AI_INFO("Conversation active, finishing conversation");
        ret = ai_conversation_finish(ai_gui->handle);
        if (ret == 0) {
            ai_gui->conversation_active = false;  // 更新状态标志
            lv_label_set_text(ai_gui->ui.status_label, "Processing...");
            lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0xF39C12), 0);
            AI_INFO("Successfully finished conversation");
        } else {
            AI_INFO("Failed to finish conversation: %d", ret);
            lv_label_set_text(ai_gui->ui.status_label, "Finish Failed");
            lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0xE74C3C), 0);
        }
    } else {
        AI_INFO("Starting new conversation");
        ret = ai_conversation_start(ai_gui->handle, NULL);
        if (ret == 0) {
            ai_gui->conversation_active = true;   // 更新状态标志
            lv_label_set_text(ai_gui->ui.status_label, "Listening...");
            lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0x3498DB), 0);
            AI_INFO("Successfully started conversation");
        } else {
            AI_INFO("Failed to start conversation: %d", ret);
            lv_label_set_text(ai_gui->ui.status_label, "Start Failed");
            lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0xE74C3C), 0);
        }
    }
}

static void clear_text_cb(lv_event_t* e)
{
    conver_gui_t* ai_gui = (conver_gui_t*)lv_event_get_user_data(e);
    lv_obj_t* obj = lv_event_get_target(e);
    if (obj == NULL) {
        return;
    }

    lv_textarea_set_text(ai_gui->ui.textarea, "");
}

static void ui_create(conver_gui_t* arg)
{
    if (arg == NULL)
    {
        return;
    }
    conver_gui_t* ai_gui = arg;
    //TODO: 字体需要优化 
    // 1. 字体资源（上实际开发板后优化）
    // 2. 字体大小（上实际开发板后优化）
    LV_IMG_DECLARE(mic);

    ai_gui->ui.textarea_font = lv_freetype_font_create("/data/res/fonts/MiSans-Normal.ttf",\
                     LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 16, LV_FREETYPE_FONT_STYLE_NORMAL);

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
    lv_label_set_text(ai_gui->ui.title, "AI Conversation");
    lv_obj_set_style_text_color(ai_gui->ui.title, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_text_font(ai_gui->ui.title, &lv_font_montserrat_32, 0);
    lv_obj_set_size(ai_gui->ui.title, LV_PCT(100), LV_PCT(8));
    lv_obj_set_style_text_align(ai_gui->ui.title, LV_TEXT_ALIGN_CENTER, 0);
    
    // Create status bar
    ai_gui->ui.status_bar = lv_obj_create(ai_gui->ui.root);
    lv_obj_set_size(ai_gui->ui.status_bar, LV_PCT(90), LV_PCT(6));
    lv_obj_set_style_bg_color(ai_gui->ui.status_bar, lv_color_hex(0xECF0F1), 0);
    lv_obj_set_style_border_color(ai_gui->ui.status_bar, lv_color_hex(0xBDC3C7), 0);
    lv_obj_set_style_border_width(ai_gui->ui.status_bar, 1, 0);
    lv_obj_set_style_radius(ai_gui->ui.status_bar, 4, 0);
    lv_obj_set_style_pad_all(ai_gui->ui.status_bar, 8, 0);
    
    // Create status label
    ai_gui->ui.status_label = lv_label_create(ai_gui->ui.status_bar);
    lv_label_set_text(ai_gui->ui.status_label, "Ready");
    lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0x27AE60), 0);
    lv_obj_set_style_text_font(ai_gui->ui.status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(ai_gui->ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(ai_gui->ui.status_label);
    
    // Create text area
    ai_gui->ui.textarea = lv_textarea_create(ai_gui->ui.root);
    lv_obj_set_size(ai_gui->ui.textarea, LV_PCT(100), LV_PCT(40));
    lv_obj_set_style_text_align(ai_gui->ui.textarea, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(ai_gui->ui.textarea, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_border_color(ai_gui->ui.textarea, lv_color_hex(0xBDC3C7),\
                                                            0);
    lv_obj_set_style_border_width(ai_gui->ui.textarea, 1, 0);
    lv_obj_set_style_radius(ai_gui->ui.textarea, 8, 0);
    lv_obj_set_style_bg_color(ai_gui->ui.textarea, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(ai_gui->ui.textarea, ai_gui->ui.textarea_font, 0);
    lv_textarea_set_placeholder_text(ai_gui->ui.textarea,\
                         "Hold the microphone button and speak...");

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
                                                LV_EVENT_CLICKED, ai_gui);
    lv_obj_add_event_cb(ai_gui->ui.clear_btnm, clear_text_cb, \
                                                LV_EVENT_CLICKED, ai_gui);

}

static void ai_conv_callback(conversation_event_t event, \
                            const conversation_result_t* result, void* cookie)
{
    conver_gui_t* ai_gui = (conver_gui_t*)cookie;

    if (!ai_gui) {
        return;
    }

    switch (event) {
    case conversation_event_start:
        lv_label_set_text(ai_gui->ui.status_label, "Conversation Started");
        lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0x27AE60), 0);
        ai_gui->conversation_active = true;  // 同步状态标志
        AI_INFO("Conversation started - ready to listen");
        break;

    case conversation_event_input_text:
        if (result && result->result) {
            lv_textarea_set_text(ai_gui->ui.textarea, result->result);
            lv_label_set_text(ai_gui->ui.status_label, "You said:");
            lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0x2C3E50), 0);
            AI_INFO("User input received: %s", result->result);
        }
        break;
        
    case conversation_event_response_text:
        if (result && result->result) {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), "AI: %s", result->result);
            lv_textarea_set_text(ai_gui->ui.textarea, buffer);
            lv_label_set_text(ai_gui->ui.status_label, "AI Responding");
            lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0x9B59B6), 0);
            AI_INFO("AI response received: %s", result->result);
        }
        break;
        
    case conversation_event_response_audio:
        if (result && result->len > 0) {
            lv_label_set_text(ai_gui->ui.status_label, "Playing AI Response");
            lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0xE67E22), 0);
            AI_INFO("AI audio response received: %d bytes", result->len);
        }
        break;
        
    case conversation_event_complete:
        lv_label_set_text(ai_gui->ui.status_label, "Conversation Complete - Click to Start New");
        lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0x27AE60), 0);
        ai_gui->conversation_active = false;  // 同步状态标志
        AI_INFO("Conversation completed successfully - ready for next round");
        break;
        
    case conversation_event_error:
        lv_label_set_text(ai_gui->ui.status_label, "Conversation Error - Click to Retry");
        lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0xE74C3C), 0);
        ai_gui->conversation_active = false;  // 同步状态标志
        if (result) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Error: %d", result->error_code);
            lv_textarea_set_text(ai_gui->ui.textarea, buffer);
            AI_ERR("Conversation error: %d\n", result ? result->error_code : -1);
        }
        break;
        
    case conversation_event_stop:
        lv_label_set_text(ai_gui->ui.status_label, "Conversation Stopped - Click to Start New");
        lv_obj_set_style_text_color(ai_gui->ui.status_label, lv_color_hex(0x95A5A6), 0);
        ai_gui->conversation_active = false;  // 同步状态标志
        AI_INFO("Conversation stopped - ready for next round");
        break;
        
    default:
        AI_INFO("Unknown conversation event: %d", event);
        break;
    }
}

static int ai_conversation_engine_init(conver_gui_t* arg)
{
    conversation_init_params_t param = {0};
    conver_gui_t* ai_gui = arg;

    param.loop = &ai_gui->ui_loop;
    param.engine_type = conversation_engine_type_volc;
    ai_gui->handle = ai_conversation_create_engine(&param);
    if (ai_gui->handle == NULL) {
        LV_LOG_ERROR("Failed to create conversation engine\n");
        return -1;
    }

    ai_conversation_set_listener(ai_gui->handle, ai_conv_callback, ai_gui);

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int main(int argc, FAR char* argv[])
{
    conver_gui_t ai_gui;
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
    
    // 初始化对话状态
    ai_gui.conversation_active = false;

    ui_create(&ai_gui);
    ai_conversation_engine_init(&ai_gui);
    // refresh lvgl ui
    lv_nuttx_uv_loop(&ai_gui.ui_loop, &result);

    lv_nuttx_deinit(&result);
    lv_deinit();

    return 0;
}
