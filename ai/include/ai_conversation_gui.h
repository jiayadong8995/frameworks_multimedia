/****************************************************************************
 * frameworks/ai/include/ai_conversation.h
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
#ifndef __AI_CONVERSATION_GUI_H
#define __AI_CONVERSATION_GUI_H
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <unistd.h>
#include <uv.h>
#include <uv_async_queue.h>
#include <lvgl/lvgl.h>


/****************************************************************************
 * Public Types
 ****************************************************************************/


typedef struct conver_gui_s
{
    uv_loop_t ui_loop;
    void* handle;
    bool conversation_active;

    struct
    {
        struct
        {
            lv_obj_t *background;
            lv_obj_t *icon_label_cont;
            lv_obj_t *status_label;
            lv_obj_t *voice_btntnm;
            lv_obj_t *result_textarea;
        }ui_components;
    
        struct
        {
            lv_font_t *size_16_normal;
            lv_font_t *size_20_normal;
            lv_font_t *size_24_normal;
            lv_font_t *size_40_normal;
        }ui_font;

        struct
        {
            lv_obj_t *mic_icon;
            lv_obj_t *app_icon;
        }ui_image;

        struct
        {
            char *font_path;
            char *mic_icon_path;
            char *app_icon_path;
        }ui_source_path;

    }ui;

}conver_gui_t;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// asr_handle_t ai_asr_create_engine(const asr_init_params_t* param);

#endif // __AI_ASR_GUI_H
