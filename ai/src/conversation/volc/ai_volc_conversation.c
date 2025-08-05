/****************************************************************************
 * frameworks/ai/src/conversation/volc/ai_volc_conversation.c
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

#include <errno.h>
#include <json_object.h>
#include <json_tokener.h>
#include <libwebsockets.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <uv.h>
#include <uv_async_queue.h>

#include "ai_common.h"
#include "ai_conversation_plugin.h"
#include "ai_ring_buffer.h"

#define VOLC_API_KEY "sk-8b5a267e4b564abcaa2943a786760427guqtb24r5z2b8vye"
#define VOLC_URL "wss://ai-gateway.vei.volces.com/v1/realtime"
#define VOLC_HOST "ai-gateway.vei.volces.com"
#define VOLC_PATH "/v1/realtime"
#define VOLC_CLIENT_PROTOCOL_NAME ""

#define VOLC_HEADER_LEN 12
#define VOLC_TIMEOUT 1000 // milliseconds
#define VOLC_BUFFER_MAX_SIZE 128 * 1024
#define VOLC_LOOP_INTERVAL 1000 // microseconds

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef enum {
    VOLC_STATE_DISCONNECTED,
    VOLC_STATE_CONNECTING,
    VOLC_STATE_CONNECTED,
    VOLC_STATE_SESSION_CREATED,
    VOLC_STATE_LISTENING,
    VOLC_STATE_PROCESSING,
    VOLC_STATE_SPEAKING,
    VOLC_STATE_ERROR
} volc_conversation_state_t;

typedef struct volc_conversation_engine {
    // WebSocket connection
    struct lws_context* lws_context;
    struct lws* wsi;

    // State management
    volc_conversation_state_t state;
    conversation_engine_callback_t event_callback;
    void* event_cookie;
    bool is_finished;  // 用户音频输入是否结束
    bool is_closed;    // 整个连接是否关闭
    bool is_running;

    // Thread and event loop
    pthread_t thread;
    uv_loop_t loop;
    sem_t sem;
    uv_async_queue_t* asyncq;
    conversation_engine_uvasyncq_cb_t uvasyncq_cb;
    void* opaque;

    // Configuration
    conversation_engine_init_params_t config;

    // Authentication
    char* api_key;

    // Send buffer
    ai_ring_buffer_t send_buffer;
    char* send_buffer_data;

    // Session data
    char* session_id;
    char* current_response_id;

    // Environment
    conversation_engine_env_params_t env;

} volc_conversation_engine_t;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int volc_conversation_websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                               void* user, void* in, size_t len);
static int volc_conversation_send_json_message(volc_conversation_engine_t* engine, json_object* json_obj);
static int volc_conversation_process_server_message(volc_conversation_engine_t* engine, const char* message);
static void volc_conversation_send_event(volc_conversation_engine_t* engine,
                                        conversation_engine_event_t event,
                                        const char* result, int len,
                                        conversation_engine_error_t error_code);
static int volc_conversation_connect_websocket(volc_conversation_engine_t* volc_engine);
static int volc_conversation_create_thread(volc_conversation_engine_t* engine);
static int volc_conversation_destroy_thread(volc_conversation_engine_t* engine);
static void* volc_conversation_uvloop_thread(void* arg);
static char* base64_encode(const unsigned char* data, size_t input_length);
static unsigned char* base64_decode(const char* data, size_t input_length, size_t* output_length);

/****************************************************************************
 * WebSocket Protocol Implementation
 ****************************************************************************/

static struct lws_protocols volc_conversation_protocols[] = {
    {
        .name = VOLC_CLIENT_PROTOCOL_NAME,
        .callback = volc_conversation_websocket_callback,
        .per_session_data_size = 0,
        .rx_buffer_size = VOLC_BUFFER_MAX_SIZE,
    },
    { NULL, NULL, 0, 0 }
};

static int volc_conversation_websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                               void* user, void* in, size_t len)
{
    volc_conversation_engine_t* engine = (volc_conversation_engine_t*)lws_context_user(lws_get_context(wsi));
    int ret;

    if (!engine) {
        AI_INFO("Engine is NULL for reason: %d", reason);
        return -1;
    }

    // 详细的回调原因映射
    const char* reason_name = NULL;
    switch (reason) {
        case LWS_CALLBACK_WSI_CREATE: reason_name = "WSI_CREATE"; break;
        case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH: reason_name = "FILTER_PRE_ESTABLISH"; break;
        case LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL: reason_name = "HTTP_BIND_PROTOCOL"; break;
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: reason_name = "APPEND_HANDSHAKE_HEADER"; break;
        case LWS_CALLBACK_CLIENT_ESTABLISHED: reason_name = "CLIENT_ESTABLISHED"; break;
        case LWS_CALLBACK_CLIENT_RECEIVE: reason_name = "CLIENT_RECEIVE"; break;
        case LWS_CALLBACK_CLIENT_WRITEABLE: reason_name = "CLIENT_WRITEABLE"; break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: reason_name = "CONNECTION_ERROR"; break;
        case LWS_CALLBACK_CLIENT_CLOSED: reason_name = "CLIENT_CLOSED"; break;
        case LWS_CALLBACK_WSI_DESTROY: reason_name = "WSI_DESTROY"; break;
        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS: reason_name = "SSL_LOAD_CERTS"; break;
        case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION: reason_name = "SSL_CERT_VERIFY"; break;
        case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED: reason_name = "SERVER_NEW_CLIENT_INSTANTIATED"; break;
        case LWS_CALLBACK_CONNECTING: reason_name = "CONNECTING"; break;
        case LWS_CALLBACK_PROTOCOL_INIT: reason_name = "PROTOCOL_INIT"; break;
        case LWS_CALLBACK_PROTOCOL_DESTROY: reason_name = "PROTOCOL_DESTROY"; break;
        case LWS_CALLBACK_HTTP: reason_name = "HTTP"; break;
        case LWS_CALLBACK_HTTP_BODY: reason_name = "HTTP_BODY"; break;
        case LWS_CALLBACK_HTTP_WRITEABLE: reason_name = "HTTP_WRITEABLE"; break;
        case LWS_CALLBACK_ADD_HEADERS: reason_name = "ADD_HEADERS"; break;
        case LWS_CALLBACK_CLIENT_HTTP_REDIRECT: reason_name = "HTTP_REDIRECT"; break;
        case LWS_CALLBACK_EVENT_WAIT_CANCELLED: reason_name = "EVENT_WAIT_CANCELLED"; break;
        default: reason_name = "UNKNOWN"; break;
    }

    // AI_INFO("websocket_callback reason: %d (%s), len: %zu", reason, reason_name, len);

    switch (reason) {
        case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
            AI_INFO("conversation_volc Pre-establish filter");
            break;

        case LWS_CALLBACK_WSI_CREATE:
            AI_INFO("conversation_volc WSI created");
            break;

        case LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL:
            AI_INFO("conversation_volc HTTP bind protocol");
            break;

        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
            {
                AI_INFO("conversation_volc Add header\n");
                unsigned char** headers = (unsigned char**)in;
                unsigned char* end = (*headers) + len;

                // Add necessary headers for authentication
                char auth_header[128];
                snprintf(auth_header, sizeof(auth_header), "Bearer %s", engine->api_key);

                AI_INFO("Adding Authorization header: Bearer %.*s...", 10, engine->api_key);

                ret = lws_add_http_header_by_name(wsi, (unsigned char*)"Authorization:",
                                                (unsigned char*)auth_header,
                                                strlen(auth_header),
                                                headers, end);
                if (ret < 0)
                    AI_INFO("Add Authorization token failed\n");

                // Add User-Agent header
                ret = lws_add_http_header_by_name(wsi,
                    (unsigned char*)"User-Agent:",
                    (unsigned char*)"curl/7.81.0",
                    strlen("curl/7.81.0"),
                    headers, end);
                if (ret < 0)
                    AI_INFO("Add User-Agent failed\n");

                // Add Accept header
                ret = lws_add_http_header_by_name(wsi,
                    (unsigned char*)"Accept:",
                    (unsigned char*)"*/*",
                    strlen("*/*"),
                    headers, end);
                if (ret < 0)
                    AI_INFO("Add Accept failed\n");
            }
            break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            AI_INFO("conversation_volc Connected to server: %s\n", VOLC_URL);
            engine->state = VOLC_STATE_CONNECTED;
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (len > 0) {
                char* message = malloc(len + 1);
                memcpy(message, in, len);
                message[len] = '\0';

                AI_INFO("Received: %.*s", (int)len, message);
                volc_conversation_process_server_message(engine, message);
                free(message);
            }
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (ai_ring_buffer_num_items(&engine->send_buffer) > 0) {
                size_t available = ai_ring_buffer_num_items(&engine->send_buffer);
                        size_t to_send = available > VOLC_BUFFER_MAX_SIZE - LWS_PRE ?
                        VOLC_BUFFER_MAX_SIZE - LWS_PRE : available;

                unsigned char* buffer = malloc(to_send + LWS_PRE);
                ai_ring_buffer_dequeue_arr(&engine->send_buffer, (char*)(buffer + LWS_PRE), to_send);

                int written = lws_write(wsi, buffer + LWS_PRE, to_send, LWS_WRITE_TEXT);
                free(buffer);

                if (written < 0) {
                    return -1;
                }

                if (ai_ring_buffer_num_items(&engine->send_buffer) > 0) {
                    lws_callback_on_writable(wsi);
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        AI_INFO("WebSocket connection error: %s", in ? (char*)in : "Unknown error");
            engine->state = VOLC_STATE_ERROR;
            const char *result = in ? (char*)in : "Connection error";
            volc_conversation_send_event(engine, conversation_engine_event_error,
                                       result, strlen(result),
                                       conversation_engine_error_network);
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            AI_INFO("WebSocket connection closed");
            engine->wsi = NULL;
            engine->state = VOLC_STATE_DISCONNECTED;
                    volc_conversation_send_event(engine, conversation_engine_event_stop,
                                     NULL, 0, conversation_engine_error_success);
            break;

        case LWS_CALLBACK_WSI_DESTROY:
            engine->wsi = NULL;
            break;

        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
            AI_INFO("conversation_volc Loading SSL certs");
            break;

        case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION:
            AI_INFO("conversation_volc SSL cert verification");
            break;

        case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
            AI_INFO("conversation_volc HTTP writeable");
            break;

        case LWS_CALLBACK_CLIENT_HTTP_REDIRECT:
            AI_INFO("conversation_volc HTTP redirect");
            break;

        case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:
            AI_INFO("conversation_volc SSL server cert verification");
            return 0;  // 跳过证书验证

        case LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY:
            AI_INFO("conversation_volc SSL context requires private key");
            break;

        case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
            AI_INFO("conversation_volc Confirm extension supported");
            break;

        case LWS_CALLBACK_WS_CLIENT_BIND_PROTOCOL:
            AI_INFO("conversation_volc WS client bind protocol");
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            AI_INFO("conversation_volc Received pong");
            break;

        default:
            AI_INFO("conversation_volc Default reason %d \n", reason);
            break;
    }

    return 0;
}

/****************************************************************************
 * JSON Message Processing
 ****************************************************************************/

static int volc_conversation_send_json_message(volc_conversation_engine_t* engine, json_object* json_obj)
{
    if (!engine || !json_obj) {
        return -EINVAL;
    }

    const char* json_string = json_object_to_json_string(json_obj);
    size_t json_len = strlen(json_string);

    if (json_len > 1024)
        // AI_INFO("Sending: %d", json_len);
        ;
    else
        AI_INFO("Sending: %s", json_string);

    if (ai_ring_buffer_is_full(&engine->send_buffer)) {
        AI_INFO("Send buffer full, clearing space");
        ai_ring_buffer_clear_arr(&engine->send_buffer, json_len);
    }

    ai_ring_buffer_queue_arr(&engine->send_buffer, json_string, json_len);
    lws_callback_on_writable(engine->wsi);

    return 0;
}

static int volc_conversation_process_server_message(volc_conversation_engine_t* engine, const char* message)
{
    json_object* json = json_tokener_parse(message);
    if (!json) {
        AI_INFO("Failed to parse JSON message");
        return -1;
    }

    json_object* type_obj;
    if (!json_object_object_get_ex(json, "type", &type_obj)) {
        json_object_put(json);
        return -1;
    }

    const char* type = json_object_get_string(type_obj);

    if (strcmp(type, "session.created") == 0) {
        json_object* session_obj;
        if (json_object_object_get_ex(json, "session", &session_obj)) {
            json_object* id_obj;
            if (json_object_object_get_ex(session_obj, "id", &id_obj)) {
                const char* session_id = json_object_get_string(id_obj);
                if (engine->session_id) {
                    free(engine->session_id);
                }
                engine->session_id = strdup(session_id);
                AI_INFO("Session created with ID: %s", session_id);
            }
        }

        engine->state = VOLC_STATE_SESSION_CREATED;
        volc_conversation_send_event(engine, conversation_engine_event_start,
            engine->session_id, strlen(engine->session_id), conversation_engine_error_success);

    } else if (strcmp(type, "input_audio_buffer.committed") == 0) {
        engine->state = VOLC_STATE_PROCESSING;

    } else if (strcmp(type, "conversation.item.input_audio_transcription.completed") == 0) {
        json_object* transcript_obj;
        if (json_object_object_get_ex(json, "transcript", &transcript_obj)) {
            const char* transcript = json_object_get_string(transcript_obj);
            volc_conversation_send_event(engine, conversation_engine_event_input_text,
                                       transcript, strlen(transcript), conversation_engine_error_success);
        }

    } else if (strcmp(type, "response.created") == 0) {
        json_object* response_obj;
        if (json_object_object_get_ex(json, "response", &response_obj)) {
            json_object* id_obj;
            if (json_object_object_get_ex(response_obj, "id", &id_obj)) {
                const char* response_id = json_object_get_string(id_obj);
                if (engine->current_response_id) {
                    free(engine->current_response_id);
                }
                engine->current_response_id = strdup(response_id);
            }
        }

        engine->state = VOLC_STATE_SPEAKING;

    } else if (strcmp(type, "response.audio.delta") == 0) {
        json_object* delta_obj;
        if (json_object_object_get_ex(json, "delta", &delta_obj)) {
            const char* audio_b64 = json_object_get_string(delta_obj);

            size_t audio_len;
            unsigned char* audio_data = base64_decode(audio_b64, strlen(audio_b64), &audio_len);

            if (audio_data) {
                volc_conversation_send_event(engine, conversation_engine_event_audio,
                                           (char*)audio_data, audio_len, conversation_engine_error_success);
                free(audio_data);
            }
        }

    } else if (strcmp(type, "response.audio_transcript.delta") == 0) {
        json_object* delta_obj;
        if (json_object_object_get_ex(json, "delta", &delta_obj)) {
            const char* text_delta = json_object_get_string(delta_obj);
            volc_conversation_send_event(engine, conversation_engine_event_text,
                                       text_delta, strlen(text_delta), conversation_engine_error_success);
        }

    } else if (strcmp(type, "response.done") == 0) {
        // 检查响应状态：完成或取消
        const char* status = "completed";  // 默认状态
        json_object* response_obj;
        if (json_object_object_get_ex(json, "response", &response_obj)) {
            json_object* status_obj;
            if (json_object_object_get_ex(response_obj, "status", &status_obj)) {
                status = json_object_get_string(status_obj);
            }
        }

        // 一轮对话完成或取消，重置状态
        engine->state = VOLC_STATE_SESSION_CREATED;

        // 根据配置决定是否自动准备下一轮
        if (engine->config.auto_next_round) {
            engine->is_finished = false;  // 自动重置音频输入标志，准备下一轮
            AI_INFO("Auto next round enabled - ready for immediate input");
        } else {
            // 保持is_finished=true，需要手动调用start来开始下一轮
            AI_INFO("Auto next round disabled - call start() for next round");
        }

        if (strcmp(status, "cancelled") == 0) {
            AI_INFO("Response cancelled by client, ready for next conversation round");
            volc_conversation_send_event(engine, conversation_engine_event_complete,
                                       "cancelled", 9, conversation_engine_error_cancelled);
        } else {
            AI_INFO("Response complete, ready for next conversation round");
            volc_conversation_send_event(engine, conversation_engine_event_complete,
                                       NULL, 0, conversation_engine_error_success);
        }

        if (engine->current_response_id) {
            free(engine->current_response_id);
            engine->current_response_id = NULL;
        }

    } else if (strcmp(type, "error") == 0) {
        json_object* error_obj;
        const char* error_message = "Unknown error";
        if (json_object_object_get_ex(json, "error", &error_obj)) {
            json_object* message_obj;
            if (json_object_object_get_ex(error_obj, "message", &message_obj)) {
                error_message = json_object_get_string(message_obj);
            }
        }

        engine->state = VOLC_STATE_ERROR;
        volc_conversation_send_event(engine, conversation_engine_event_error,
                                   error_message, strlen(error_message), conversation_engine_error_server);
    }

    json_object_put(json);
    return 0;
}

/****************************************************************************
 * Utility Functions
 ****************************************************************************/

static void volc_conversation_send_event(volc_conversation_engine_t* engine,
                                        conversation_engine_event_t event,
                                        const char* result, int len,
                                        conversation_engine_error_t error_code)
{
    if (!engine || !engine->event_callback) {
        return;
    }

    // ✅ 学习ASR/TTS架构：在WebSocket线程中直接调用回调
    // 回调函数 conversation_engine_event_cb 会负责异步队列处理
    conversation_engine_result_t engine_result = {
        .result = result,
        .len = len,
        .error_code = error_code
    };

    AI_INFO("🎯 Sending event: event=%d, result_len=%d", event, len);
    engine->event_callback(event, &engine_result, engine->event_cookie);
}

// Base64编码实现（简化版）
static char* base64_encode(const unsigned char* data, size_t input_length)
{
    static const char encoding_table[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3',
        '4', '5', '6', '7', '8', '9', '+', '/'
    };

    size_t output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = malloc(output_length + 1);
    if (!encoded_data) return NULL;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    static const int mod_table[] = {0, 2, 1};
    for (int i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[output_length - 1 - i] = '=';

    encoded_data[output_length] = '\0';
    return encoded_data;
}

// Base64解码实现（简化版）
static unsigned char* base64_decode(const char* data, size_t input_length, size_t* output_length)
{
    if (input_length % 4 != 0) return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    // 简化实现，实际项目中应使用更完整的解码
    unsigned char* decoded_data = malloc(*output_length);
    if (!decoded_data) return NULL;

    // 这里应该实现完整的base64解码逻辑
    // 为简化起见，暂时使用占位实现
    memset(decoded_data, 0, *output_length);

    return decoded_data;
}

/****************************************************************************
 * Plugin Interface Implementation
 ****************************************************************************/

static int volc_conversation_init(void* engine, const conversation_engine_init_params_t* param)
{
    volc_conversation_engine_t* volc_engine = (volc_conversation_engine_t*)engine;

    if (!volc_engine || !param) {
        return -EINVAL;
    }

    AI_INFO("Initializing VolcEngine conversation");

    // 复制配置
    memcpy(&volc_engine->config, param, sizeof(conversation_engine_init_params_t));

    // 设置认证信息
    volc_engine->api_key = param->api_key ? strdup(param->api_key) : strdup(VOLC_API_KEY);

    // 初始化发送缓冲区
    volc_engine->send_buffer_data = malloc(VOLC_BUFFER_MAX_SIZE);
    if (!volc_engine->send_buffer_data) {
        return -ENOMEM;
    }
    ai_ring_buffer_init(&volc_engine->send_buffer, volc_engine->send_buffer_data, VOLC_BUFFER_MAX_SIZE);

    // 设置环境参数
    volc_engine->env.loop = param->loop;
    volc_engine->env.format = "format=s16le:sample_rate=16000:ch_layout=mono";
    volc_engine->env.force_format = 1;

    volc_engine->state = VOLC_STATE_DISCONNECTED;

    // 初始化状态标志
    volc_engine->is_finished = false;
    volc_engine->is_closed = false;
    volc_engine->is_running = false;

    // 初始化async queue相关
    volc_engine->uvasyncq_cb = param->cb;
    volc_engine->opaque = param->opaque;

    // 创建UV循环线程
    int ret = volc_conversation_create_thread(volc_engine);
    if (ret < 0) {
        AI_INFO("Failed to create UV loop thread");
        free(volc_engine->send_buffer_data);
        free(volc_engine->api_key);
        return ret;
    }

    AI_INFO("VolcEngine conversation initialized");
    return 0;
}

static int volc_conversation_uninit(void* engine)
{
    volc_conversation_engine_t* volc_engine = (volc_conversation_engine_t*)engine;

    if (!volc_engine) {
        return -EINVAL;
    }

    AI_INFO("Uninitializing VolcEngine conversation");

    // 设置关闭和完成标志，立即停止所有操作
    volc_engine->is_finished = true;
    volc_engine->is_closed = true;

    // 销毁UV循环线程
    volc_conversation_destroy_thread(volc_engine);

    // 关闭连接（线程销毁时已处理，但为了安全起见保留）
    if (volc_engine->wsi) {
        lws_close_reason(volc_engine->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
        volc_engine->wsi = NULL;
    }

    if (volc_engine->lws_context) {
        lws_context_destroy(volc_engine->lws_context);
        volc_engine->lws_context = NULL;
    }

    // 清理内存
    if (volc_engine->send_buffer_data) {
        free(volc_engine->send_buffer_data);
    }
    if (volc_engine->session_id) {
        free(volc_engine->session_id);
    }
    if (volc_engine->current_response_id) {
        free(volc_engine->current_response_id);
    }
    if (volc_engine->api_key) {
        free(volc_engine->api_key);
    }

    AI_INFO("VolcEngine conversation uninitialized");
    return 0;
}

static int volc_conversation_event_cb(void* engine, conversation_engine_callback_t callback, void* cookie)
{
    volc_conversation_engine_t* volc_engine = (volc_conversation_engine_t*)engine;

    if (!volc_engine) {
        return -EINVAL;
    }

    volc_engine->event_callback = callback;
    volc_engine->event_cookie = cookie;

    return 0;
}

static int volc_conversation_start(void* engine, const conversation_engine_audio_info_t* audio_info)
{
    volc_conversation_engine_t* volc_engine = (volc_conversation_engine_t*)engine;

    if (!volc_engine) {
        return -EINVAL;
    }

    AI_INFO("Starting VolcEngine conversation");

    // 重置音频输入标志，开始新一轮对话
    volc_engine->is_finished = false;
    AI_INFO("Audio input enabled for new conversation round");

    // 确保WebSocket连接可用 (复用已有连接或创建新连接)
    int ret = volc_conversation_connect_websocket(volc_engine);
    if (ret < 0) {
        AI_INFO("Failed to ensure WebSocket connection");
        return ret;
    }

    return 0;
}

static int volc_conversation_connect_websocket(volc_conversation_engine_t* volc_engine)
{
    AI_INFO("Creating WebSocket connection in UV thread");

    // ✅ 检查是否已有活跃连接
    if (volc_engine->lws_context && volc_engine->wsi) {
        AI_INFO("WebSocket connection already active, reusing existing connection");
        return 0;
    }

    // ✅ 清理可能存在的旧连接
    if (volc_engine->lws_context) {
        AI_INFO("Cleaning up old WebSocket context before creating new one");
        lws_context_destroy(volc_engine->lws_context);
        volc_engine->lws_context = NULL;
        volc_engine->wsi = NULL;
    }

    // 创建WebSocket上下文
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = volc_conversation_protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.user = volc_engine;

    volc_engine->lws_context = lws_create_context(&info);
    if (!volc_engine->lws_context) {
        AI_INFO("Failed to create WebSocket context");
        return -1;
    }

    // 构建连接信息
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));

    ccinfo.context = volc_engine->lws_context;
    ccinfo.address = VOLC_HOST;
    ccinfo.port = 443;
    ccinfo.path = VOLC_PATH "?model=AG-voice-chat-agent";
    ccinfo.host = VOLC_HOST;
    ccinfo.origin = VOLC_HOST;
    ccinfo.protocol = volc_conversation_protocols[0].name;
    ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;

    // 发起连接
    volc_engine->wsi = lws_client_connect_via_info(&ccinfo);
    if (!volc_engine->wsi) {
        AI_INFO("Failed to initiate WebSocket connection");
        lws_context_destroy(volc_engine->lws_context);
        volc_engine->lws_context = NULL;
        return -1;
    }

    volc_engine->state = VOLC_STATE_CONNECTING;

    AI_INFO("WebSocket connection initiated");
    return 0;
}

static int volc_conversation_write_audio(void* engine, const char* data, int len)
{
    volc_conversation_engine_t* volc_engine = (volc_conversation_engine_t*)engine;

    if (!volc_engine || !data || len <= 0) {
        return -EINVAL;
    }

    // 如果音频输入已完成或连接已关闭，不再处理新的音频数据
    if (volc_engine->is_finished || volc_engine->is_closed) {
        return 0;
    }

    // 检查连接状态 - 在session创建后和listening状态都可以发送音频
    if (volc_engine->state != VOLC_STATE_SESSION_CREATED &&
        volc_engine->state != VOLC_STATE_LISTENING) {
        return 0;
    }

    // Base64编码音频数据
    char* audio_b64 = base64_encode((const unsigned char*)data, len);
    if (!audio_b64) {
        return -ENOMEM;
    }

    // 构建JSON消息
    json_object* json = json_object_new_object();
    json_object_object_add(json, "type", json_object_new_string("input_audio_buffer.append"));
    json_object_object_add(json, "audio", json_object_new_string(audio_b64));

    int ret = volc_conversation_send_json_message(volc_engine, json);

    json_object_put(json);
    free(audio_b64);

    if (ret == 0 && volc_engine->state == VOLC_STATE_SESSION_CREATED) {
        volc_engine->state = VOLC_STATE_LISTENING;
        AI_INFO("State changed to LISTENING, ready for continuous audio");
    }

    return ret;
}

static int volc_conversation_finish(void* engine)
{
    volc_conversation_engine_t* volc_engine = (volc_conversation_engine_t*)engine;

    if (!volc_engine) {
        return -EINVAL;
    }

    // 设置音频输入完成标志（但保持连接以接收服务端响应）
    volc_engine->is_finished = true;
    volc_engine->state = VOLC_STATE_PROCESSING;

    // 提交音频缓冲区 - 使用正确的协议格式
    json_object* commit_json = json_object_new_object();
    json_object_object_add(commit_json, "type", json_object_new_string("input_audio_buffer.commit"));

    int ret = volc_conversation_send_json_message(volc_engine, commit_json);
    json_object_put(commit_json);

    if (ret < 0) {
        return ret;
    }

    // 请求响应 - 使用正确的协议格式
    json_object* json = json_object_new_object();
    json_object_object_add(json, "type", json_object_new_string("response.create"));
    json_object* response_json = json_object_new_object();
    json_object* modalities_json = json_object_new_array();
    json_object_array_add(modalities_json, json_object_new_string("text"));
    json_object_array_add(modalities_json, json_object_new_string("audio"));
    json_object_object_add(response_json, "modalities", modalities_json);
    json_object_object_add(json, "response", response_json);

    ret = volc_conversation_send_json_message(volc_engine, json);
    json_object_put(json);

    return ret;
}

static int volc_conversation_cancel(void* engine)
{
    volc_conversation_engine_t* volc_engine = (volc_conversation_engine_t*)engine;

    if (!volc_engine) {
        return -EINVAL;
    }

    volc_engine->is_finished = true;

    json_object* json = json_object_new_object();
    json_object_object_add(json, "type", json_object_new_string("response.cancel"));

    int ret = volc_conversation_send_json_message(volc_engine, json);
    json_object_put(json);

    AI_INFO("Cancel: sent response.cancel, waiting for server response.done with cancelled status");

    return ret;
}

static conversation_engine_env_params_t* volc_conversation_get_env(void* engine)
{
    volc_conversation_engine_t* volc_engine = (volc_conversation_engine_t*)engine;

    if (!volc_engine) {
        return NULL;
    }

    return &volc_engine->env;
}

/****************************************************************************
 * Thread Management
 ****************************************************************************/

static void* volc_conversation_uvloop_thread(void* arg)
{
    volc_conversation_engine_t* engine = (volc_conversation_engine_t*)arg;
    int ret;

    ret = uv_loop_init(&engine->loop);
    if (ret < 0) {
        AI_INFO("Failed to init UV loop");
        return NULL;
    }

    if (engine->uvasyncq_cb) {
        engine->asyncq = (uv_async_queue_t*)malloc(sizeof(uv_async_queue_t));
        engine->asyncq->data = engine->opaque;
        ret = uv_async_queue_init(&engine->loop, engine->asyncq, engine->uvasyncq_cb);
        if (ret < 0)
            goto out;
        AI_INFO("conversation_asyncq_init:%p", engine->asyncq);
    }

    AI_INFO("Conversation UV loop running: %d", uv_loop_alive(&engine->loop));

    while (uv_loop_alive(&engine->loop) && !engine->is_closed) {
        ret = uv_run(&engine->loop, UV_RUN_NOWAIT);
        if (ret == 0 && !engine->is_closed) {
            break; // 正常退出
        }

        // Service WebSocket events - 只要连接未关闭就继续服务
        if (!engine->is_closed && engine->lws_context) {
            ret = lws_service(engine->lws_context, -1);
            if (ret < 0) {
                AI_INFO("conversation lws_service failed: %d", ret);
                volc_conversation_send_event(engine, conversation_engine_event_error,
                                           "WebSocket service error", 23,
                                           conversation_engine_error_network);
                break;
            }
        } else if (engine->is_closed && engine->lws_context) {
            // Cleanup when connection is explicitly closed
            lws_context_destroy(engine->lws_context);
            engine->lws_context = NULL;
            engine->wsi = NULL;
            AI_INFO("conversation service stopped");
            break;
        }

        if (!engine->is_running) {
            sem_post(&engine->sem);
            engine->is_running = true;
        }

        usleep(VOLC_LOOP_INTERVAL);
    }

    sem_post(&engine->sem);

    // Cleanup
    if (engine->lws_context) {
        lws_context_destroy(engine->lws_context);
        engine->lws_context = NULL;
        engine->wsi = NULL;
    }

out:
    if (engine->asyncq) {
        free(engine->asyncq);
        engine->asyncq = NULL;
    }
    ret = uv_loop_close(&engine->loop);
    engine->is_running = false;
    AI_INFO("Conversation UV loop thread ended: %d", ret);
    return NULL;
}

static int volc_conversation_create_thread(volc_conversation_engine_t* engine)
{
    struct sched_param param;
    pthread_attr_t attr;
    int ret;

    AI_INFO("Creating conversation UV loop thread");

    ret = sem_init(&engine->sem, 0, 0);
    if (ret < 0) {
        AI_INFO("Failed to init semaphore");
        return ret;
    }

    engine->is_closed = false;
    engine->is_running = false;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16384);
    param.sched_priority = 110;
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&engine->thread, &attr, volc_conversation_uvloop_thread, engine);
    if (ret != 0) {
        AI_INFO("pthread_create failed");
        sem_destroy(&engine->sem);
        return ret;
    }

    pthread_setname_np(engine->thread, "ai_conv_volc");
    pthread_attr_destroy(&attr);

    // Wait for thread to start
    sem_wait(&engine->sem);

    AI_INFO("Conversation UV loop thread created successfully");
    return 0;
}

static int volc_conversation_destroy_thread(volc_conversation_engine_t* engine)
{
    AI_INFO("Destroying conversation UV loop thread");

    engine->is_closed = true;

    // Wait for thread to finish
    if (engine->is_running) {
        sem_wait(&engine->sem);
    }

    sem_destroy(&engine->sem);
    AI_INFO("Conversation UV loop thread destroyed");
    return 0;
}

/****************************************************************************
 * Plugin Definition
 ****************************************************************************/

conversation_engine_plugin_t volc_conversation_engine_plugin = {
    .name = "volc_conversation",
    .priv_size = sizeof(volc_conversation_engine_t),
    .init = volc_conversation_init,
    .uninit = volc_conversation_uninit,
    .event_cb = volc_conversation_event_cb,
    .start = volc_conversation_start,
    .write_audio = volc_conversation_write_audio,
    .finish = volc_conversation_finish,
    .cancel = volc_conversation_cancel,
    .get_env = volc_conversation_get_env,
};