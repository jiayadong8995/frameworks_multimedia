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
#ifndef __AI_LVGL_H
#define __AI_LVGL_H
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <unistd.h>
#include <uv.h>
#include <uv_async_queue.h>

// include lvgl headers
#include <lvgl/lvgl.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/
#define SCREEN_WIDTH    (lv_obj_get_width(lv_scr_act()))
#define SCREEN_HEIGHT   (lv_obj_get_height(lv_scr_act()))

#define DEMO_WIDTH (int32_t)((SCREEN_HEIGHT * 0.95f))
#define DEMO_HEIGHT (int32_t)(DEMO_WIDTH)

typedef enum asr_status_e
{
    ASR_STATUS_IDLE = 0,
    ASR_STATUS_PROCESSING,
    ASR_STATUS_FINISHED,
    ASR_STATUS_ERROR,
    ASR_STATUS_XXXX
}asr_status_t;

typedef struct asr_ope_s {
    int id;
    void* handle;
    void* extra;
    int handle_type;
} asr_ope_t;

typedef struct asr_thread_s
{
    pthread_attr_t* attr;
    pthread_t* ai_uvloop_tid;

    uv_loop_t* asrloop;
    uv_async_queue_t* asyncq;
    uv_timer_t timer;

    asr_ope_t asr_ope;

}asr_thread_t;

typedef struct ai_gui_s
{
    asr_status_t status; // Status of the audio text processing
    const char *asr_result_text; // Pointer to the ASR result text

    uv_loop_t ui_loop;

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


#endif // __AI_LVGL_H
