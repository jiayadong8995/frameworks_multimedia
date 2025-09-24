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

#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <ai_conversation_gui.h>
#include <ai_conversation.h>
#include <ai_common.h>

/****************************************************************************
 * Private Types
 ****************************************************************************/
#define SCREEN_WIDTH    (lv_obj_get_width(lv_scr_act()))
#define SCREEN_HEIGHT   (lv_obj_get_height(lv_scr_act()))

#define DEMO_WIDTH (int32_t)((SCREEN_HEIGHT * 1.0f))
#define DEMO_HEIGHT (int32_t)(DEMO_WIDTH)

#define DEMO(x) (DEMO_WIDTH * (x) / 100)

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
            ai_gui->conversation_active = false;
            lv_label_set_text(ai_gui->ui.ui_components.status_label, "回应中...");
            AI_INFO("Successfully finished conversation");
        } else {
            AI_INFO("Failed to finish conversation: %d", ret);
            lv_label_set_text(ai_gui->ui.ui_components.status_label, "完成失败");
        }
    } else {
        lv_textarea_set_text(ai_gui->ui.ui_components.result_textarea, "");
        AI_INFO("Starting new conversation");
        ret = ai_conversation_start(ai_gui->handle, NULL);
        if (ret == 0) {
            ai_gui->conversation_active = true;
            lv_label_set_text(ai_gui->ui.ui_components.status_label, "倾听中...");
            AI_INFO("Successfully started conversation");
        } else {
            AI_INFO("Failed to start conversation: %d", ret);
            lv_label_set_text(ai_gui->ui.ui_components.status_label, "开始失败");
        }
    }
}

static void ui_resource_init(conver_gui_t* arg)
{
    if (arg == NULL)
    {
        return;
    }
    conver_gui_t* ai_gui = arg;
    ai_gui->ui.ui_source_path.font_path = "/data/res/fonts/MiSans-Normal.ttf";
    ai_gui->ui.ui_source_path.mic_icon_path = "/data/res/icons/volc.png";
    ai_gui->ui.ui_source_path.app_icon_path = "/data/res/icons/app.png";
    if(ai_gui->ui.ui_source_path.font_path == NULL ||
       ai_gui->ui.ui_source_path.mic_icon_path == NULL ||
       ai_gui->ui.ui_source_path.app_icon_path == NULL) {
        LV_LOG_ERROR("Failed to set resource path\n");
        return;
    }
    //TODO: 字体需要优化 
    // 1. 字体资源（上实际开发板后优化）
    // 2. 字体大小（上实际开发板后优化）
    ai_gui->ui.ui_font.size_16_normal = lv_freetype_font_create(ai_gui->ui.ui_source_path.font_path,\
                     LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 16, LV_FREETYPE_FONT_STYLE_NORMAL);
    ai_gui->ui.ui_font.size_20_normal = lv_freetype_font_create(ai_gui->ui.ui_source_path.font_path,\
                     LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 20, LV_FREETYPE_FONT_STYLE_NORMAL);
    ai_gui->ui.ui_font.size_24_normal = lv_freetype_font_create(ai_gui->ui.ui_source_path.font_path,\
                     LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 24, LV_FREETYPE_FONT_STYLE_NORMAL);
    ai_gui->ui.ui_font.size_40_normal = lv_freetype_font_create(ai_gui->ui.ui_source_path.font_path,\
                     LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 40, LV_FREETYPE_FONT_STYLE_NORMAL);

    if (ai_gui->ui.ui_font.size_16_normal == NULL ||
        ai_gui->ui.ui_font.size_20_normal == NULL ||
        ai_gui->ui.ui_font.size_40_normal == NULL) {
        LV_LOG_ERROR("Failed to create text area font\n");
        return;
    }
}

static void ui_create(conver_gui_t* arg)
{
    if (arg == NULL)
    {
        return;
    }
    conver_gui_t* ai_gui = arg;

    ui_resource_init(ai_gui);

    ai_gui->ui.ui_components.background = lv_obj_create(lv_scr_act());
    lv_obj_center(ai_gui->ui.ui_components.background);
    lv_obj_set_style_bg_color(ai_gui->ui.ui_components.background,\
                                     lv_color_black(), LV_PART_MAIN);
    lv_obj_set_size(ai_gui->ui.ui_components.background, DEMO_WIDTH, DEMO_WIDTH);
    lv_obj_remove_flag(ai_gui->ui.ui_components.background, LV_OBJ_FLAG_SCROLLABLE);

    ai_gui->ui.ui_components.icon_label_cont = \
                                    lv_obj_create(ai_gui->ui.ui_components.background);
    lv_obj_set_size(ai_gui->ui.ui_components.icon_label_cont, DEMO(40), DEMO(15));
    lv_obj_align(ai_gui->ui.ui_components.icon_label_cont, LV_ALIGN_TOP_LEFT, \
                    DEMO(3), DEMO(3));
    lv_obj_set_style_bg_opa(ai_gui->ui.ui_components.icon_label_cont,\
                             LV_PART_MAIN, LV_OPA_TRANSP);
    lv_obj_set_style_border_color(ai_gui->ui.ui_components.icon_label_cont,\
                                 lv_color_black(), LV_PART_MAIN);
    lv_obj_remove_flag(ai_gui->ui.ui_components.icon_label_cont, LV_OBJ_FLAG_SCROLLABLE);

    ai_gui->ui.ui_image.app_icon = lv_img_create(ai_gui->ui.ui_components.icon_label_cont);
    lv_img_set_src(ai_gui->ui.ui_image.app_icon, ai_gui->ui.ui_source_path.app_icon_path);
    lv_obj_set_size(ai_gui->ui.ui_image.app_icon, DEMO(10), DEMO(10));
    lv_obj_align(ai_gui->ui.ui_image.app_icon, LV_ALIGN_LEFT_MID, 0, 0);
    lv_img_set_zoom(ai_gui->ui.ui_image.app_icon, 75);

    ai_gui->ui.ui_components.status_label = \
                                lv_label_create(ai_gui->ui.ui_components.icon_label_cont);
    lv_obj_set_style_text_font(ai_gui->ui.ui_components.status_label,\
                    ai_gui->ui.ui_font.size_24_normal, 0);
    lv_obj_set_style_text_color(ai_gui->ui.ui_components.status_label, \
                            lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(ai_gui->ui.ui_components.status_label, "待命中...");
    lv_obj_align(ai_gui->ui.ui_components.status_label, \
            LV_ALIGN_LEFT_MID, DEMO(12), 0);

    ai_gui->ui.ui_components.result_textarea = \
                                lv_textarea_create(ai_gui->ui.ui_components.background);
    lv_obj_set_size(ai_gui->ui.ui_components.result_textarea, DEMO(100), DEMO(30));
    lv_obj_set_style_text_align(ai_gui->ui.ui_components.result_textarea, \
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ai_gui->ui.ui_components.result_textarea,\
                    ai_gui->ui.ui_font.size_40_normal, 0);
    lv_obj_set_style_border_color(ai_gui->ui.ui_components.result_textarea,\
                                 lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ai_gui->ui.ui_components.result_textarea,\
                                lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(ai_gui->ui.ui_components.result_textarea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_color(ai_gui->ui.ui_components.result_textarea, \
                            lv_color_white(), LV_PART_MAIN);
    lv_textarea_set_text(ai_gui->ui.ui_components.result_textarea, "我在听，请你按下按键说");
    lv_obj_align(ai_gui->ui.ui_components.result_textarea, LV_ALIGN_CENTER, 0, 0);

    ai_gui->ui.ui_components.voice_btntnm = \
                            lv_btn_create(ai_gui->ui.ui_components.background);
    lv_obj_align(ai_gui->ui.ui_components.voice_btntnm, LV_ALIGN_CENTER, \
                    0, DEMO(20));
    lv_obj_set_size(ai_gui->ui.ui_components.voice_btntnm, DEMO(15), DEMO(15));
    lv_obj_set_style_bg_color(ai_gui->ui.ui_components.voice_btntnm,\
                             lv_color_black(), 0);
    lv_obj_set_style_border_color(ai_gui->ui.ui_components.voice_btntnm,\
                                 lv_color_black(), 0);
    
    // Add event callbacks
    lv_obj_add_event_cb(ai_gui->ui.ui_components.voice_btntnm, key_press_cb, \
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
        lv_label_set_text(ai_gui->ui.ui_components.status_label, "对话开始");
        ai_gui->conversation_active = true;
        AI_INFO("Conversation started - ready to listen");
        break;

    case conversation_event_input_text:
        if (result && result->result) {
            AI_INFO("User input received: %s", result->result);
            lv_textarea_add_text(ai_gui->ui.ui_components.result_textarea, result->result);
            lv_label_set_text(ai_gui->ui.ui_components.status_label, "你说:");
        }
        break;

    case conversation_event_response_text:
        if (result && result->result) {
            AI_INFO("AI response received: %s", result->result);
            lv_textarea_add_text(ai_gui->ui.ui_components.result_textarea, result->result);
            lv_label_set_text(ai_gui->ui.ui_components.status_label, "回应中...");
        }
        break;

    case conversation_event_response_audio:
        if (result && result->len > 0) {
            AI_INFO("AI audio response received: %d bytes", result->len);
            lv_label_set_text(ai_gui->ui.ui_components.status_label, "播放中...");
        }
        break;
        
    case conversation_event_complete:
        AI_INFO("Conversation completed successfully - ready for next round");
        lv_label_set_text(ai_gui->ui.ui_components.status_label, "交流完成");
        ai_gui->conversation_active = false;
        break;
        
    case conversation_event_error:
        lv_label_set_text(ai_gui->ui.ui_components.status_label, "对话错误");
        ai_gui->conversation_active = false;
        if (result) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Error: %d", result->error_code);
            AI_ERR("Conversation error: %d\n", result ? result->error_code : -1);
        }
        break;
        
    case conversation_event_stop:
        lv_label_set_text(ai_gui->ui.ui_components.status_label, "对话停止");
        ai_gui->conversation_active = false;
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
