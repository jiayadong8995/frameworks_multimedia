/****************************************************************************
 * frameworks/ai/include/ai_asr_gui.h
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
#ifndef __AI_ASR_GUI_H
#define __AI_ASR_GUI_H
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <unistd.h>
#include <uv.h>
#include <uv_async_queue.h>
#include <lvgl/lvgl.h>

#include "ai_asr_internal.h"
#include "ai_asr.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct ai_gui_s
{
    uv_loop_t ui_loop;
    void* handle;
    struct
    {
        lv_obj_t *root;
        lv_obj_t *title;
        lv_obj_t *clear_btnm;
        lv_obj_t *clear_btnm_img;
        lv_obj_t *voice_btntnm;
        lv_obj_t *voice_btntnm_img;
        lv_obj_t *textarea;
        const lv_font_t *textarea_font;
    }ui;

}ai_gui_t;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// asr_handle_t ai_asr_create_engine(const asr_init_params_t* param);

#endif // __AI_ASR_GUI_H
