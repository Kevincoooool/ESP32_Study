#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_wifi.h"
#include "esp_camera.h"
#include "mqtt_client.h"
#include "esp_http_client.h"

#define MAX_HTTP_RECV_BUFFER 512

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static const char *TAG = "example:take_picture";

char deviceUUID[17];

//mqtt
static esp_mqtt_client_handle_t client;

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_VGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 10, //0-63 lower number means higher quality
    .fb_count = 1       //if more than one, i2s runs in continuous mode. Use only with JPEG
};

static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // Write out data
            // printf("%.*s", evt->data_len, (char*)evt->data);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

static void http_test_task(void *pvParameters)
{

    ESP_LOGI(TAG, "Http Start");

    camera_fb_t *pic = NULL;
    esp_err_t res = ESP_OK;
    size_t fb_len = 0;

    //配置服务器相关信息
    esp_http_client_config_t config = {
        .url = "http://www.xuhongv.com/index.php",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .buffer_size = 4096,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    //开始拍照
    pic = esp_camera_fb_get();

    if (!pic)
    {
        ESP_LOGE(TAG, "Camera capture failed");
    }
    else
    {
        //拍照成功，获取其大小、尺寸等信息
        ESP_LOGI(TAG, "Camera capture OK , Its size was: %zu bytes", pic->len);
        ESP_LOGI(TAG, "Camera capture OK , Its width was: %d", pic->width);
        ESP_LOGI(TAG, "Camera capture OK , Its height was: %d ", pic->height);

        //设置HTTP请求头为image/jpg表示图片类型
        res = esp_http_client_set_header(client, "Content-Type", "image/jpg");
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_http_client_set_header result code : [%s]", esp_err_to_name(res));
            goto end;
        }

        //把图片放在body里面
        res = esp_http_client_set_post_field(client, (const char *)pic->buf, pic->len);
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_http_client_set_post_field result  code : [%s]", esp_err_to_name(res));
            goto end;
        }

        vTaskDelay(500 / portTICK_RATE_MS);

        //开始执行请求服务器
        res = esp_http_client_perform(client);

        //判断是否请求成功
        if (res == ESP_OK)
        {
            ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
        }
        else
        {
            ESP_LOGE(TAG, "perform http request %s", esp_err_to_name(res));
        }
    }

end:
{
    esp_http_client_cleanup(client);
    esp_camera_fb_return(pic);
    ESP_LOGI(TAG, "Http End");
}

    vTaskDelete(NULL);
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:

        msg_id = esp_mqtt_client_subscribe(client, deviceUUID, 1);
        ESP_LOGI(TAG, "sent subscribe successful, deviceUUID=%s", deviceUUID);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        //开始拍照上传
        xTaskCreate(&http_test_task, "http_test_task", 8192, NULL, 5, NULL);
        printf("     esp_get_free_heap_size : %d  \n", esp_get_free_heap_size());
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

/* 
 * @Description: MQTT参数连接的配置
 * @param: 
 * @return: 
*/
void TaskXMqttRecieve(void *p)
{
    //连接的配置参数
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = "aligenie.xuhongv.com", //连接的域名 ，请务必修改为您的
        .port = 1883,                   //端口，请务必修改为您的
        .username = "admin",            //用户名，请务必修改为您的
        .password = "xuhong123",        //密码，请务必修改为您的
        .client_id = deviceUUID,
        .event_handle = mqtt_event_handler, //设置回调函数
        .keepalive = 120,                   //心跳
        .disable_auto_reconnect = false,    //开启自动重连
        .disable_clean_session = false,     //开启 清除会话
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

    vTaskDelete(NULL);
}

void app_main()
{

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    printf("\n\n-------------------------------- Get Systrm Info------------------------------------------\n");
    //获取IDF版本
    printf("     SDK version:%s\n", esp_get_idf_version());
    //获取芯片可用内存
    printf("     esp_get_free_heap_size : %d  \n", esp_get_free_heap_size());
    //获取从未使用过的最小内存
    printf("     esp_get_minimum_free_heap_size : %d  \n", esp_get_minimum_free_heap_size());
    //获取mac地址（station模式）
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(deviceUUID, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    app_wifi_initialise();

    init_camera();

    //等待Wi-Fi连接成功
    app_wifi_wait_connected();

    xTaskCreate(TaskXMqttRecieve, "TaskXMqttRecieve", 1024 * 4, NULL, 5, NULL);
}