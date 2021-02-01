#ifndef __VT_HANDLE_H__
#define __VT_HANDLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "esp_audio.h"

#define RELEASE(ptr, action) do {\
		if (!ptr) break;\
		action(ptr);\
		ptr = NULL;\
	} while (0)\

#define AUDIO_STR_CHECK(TAG, a, action, fmt, ...) do {                                          \
        if (!(a)) {                                                                             \
        ESP_LOGE(TAG,"%s:%d (%s)"fmt, __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__);     \
        action;                                                                                 \
		}                                                                                       \
    } while (0)                                                                                 \

// #define USE_MORE_TASK         // send picture to vt_server via creating two task 
// #define PRINTF_CAMERA_TIME

esp_err_t  esp_vt_handle_stop();

esp_err_t  esp_vt_handle_start();

int  esp_vt_handle_state();

esp_err_t esp_vt_handle_login_in();

esp_err_t esp_vt_handle_pause();

esp_err_t esp_vt_handle_resume();

esp_err_t esp_vt_handle_init();

#ifdef __cplusplus
}
#endif

#endif //__VT_HANDLE_H__

