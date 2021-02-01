# take_pic_from_mqtt

# 目录

- [1.功能描述](#Funtion)  
- [2.准备](#hardwareprepare)  
- [3.服务器准备](#clouds)  
- [4.嵌入式代码详解](#device)  
- [5.推荐开源好玩DIY的一览表](#other)  

## <span id = "Funtion">一、功能描述</span>

本文基于 linux 环境，通过安信可 ESP32-Cam 开发板 SDK C语言编程二次开发，通过本地局域网查阅摄像头的视频流。

## <span id = "Introduction">二、准备</span>

### 硬件

- 安信可 ESP32-Cam 开发板：https://item.taobao.com/item.htm?id=573698917181

- TTL-USB 调试工具（推荐使用这个，保证足够的电压电流）：https://item.taobao.com/item.htm?id=565546260974

### 软件

- 环境搭建：[Linux环境搭建 /relese/v3.3.2 分支](https://docs.espressif.com/projects/esp-idf/zh_CN/v3.3.2/get-started/linux-setup.html)
- 工具链设置：下载 toolchain，博主使用的版本是：```gcc version 5.2.0 (crosstool-NG crosstool-ng-1.22.0-97-gc752ad5)```
- Python版本：`Python 2.7.17`

## <span id = "clouds">三、设置路由器SSID和密码

在 `app_wifi.c`文件的路由器账号密码，其格式如下：

```c
void app_wifi_initialise(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "aiot@xuhongv",
            .password = "12345678",
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
```

## <span id = "device">四、日志打印

文件目录说明：

```
├─1_take_pic_http_to_cloud 
│ ├─components 摄像头驱动代码组件
│ ├─main 用户程序
│ │ ├─app_wifi.c/h 连接路由器的逻辑实现
│ │ ├─main.c 主文件程序入口
```

LOG日志打印，可以看到在本地局域网的IP地址是：`10.10.10.114`，要访问地址为：`10.10.10.114:80/stream`

```c
I (479) gpio: GPIO[32]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (629) camera: Detected OV2640 camera
I (639) camera: Allocating 1 frame buffers (60 KB total)
I (639) camera: Allocating 60 KB frame buffer in OnBoard RAM
I (1479) wifi:new:<9,0>, old:<1,0>, ap:<255,255>, sta:<9,0>, prof:1
I (2469) wifi:state: init -> auth (b0)
I (2479) wifi:state: auth -> assoc (0)
I (2479) wifi:state: assoc -> run (10)
I (2769) wifi:connected with aiot@xuhongv, aid = 1, channel 9, BW20, bssid = d8:c8:e9:05:c4:d8
I (2769) wifi:security type: 4, phy: bgn, rssi: -37
I (2769) wifi:pm start, type: 1

I (2839) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (3699) event: sta ip: 10.10.10.114, mask: 255.255.255.0, gw: 10.10.10.1
I (3699) example:take_picture: Http Start
I (3699) example:take_picture: Starting server on port: '80'
I (3699) example:take_picture: Registering URI handlers
I (3709) example:take_picture: Http End
```

## <span id = "device">五、推荐开源好玩DIY的一览表

| 开源项目                                                 | 地址                                                        | 开源时间   |
| -------------------------------------------------------- | ----------------------------------------------------------- | ---------- |
| ESP32-Cam摄像头拍照上传到私有服务器                      |                                                             | 2020.12.30 |
| 微信小程序连接mqtt服务器，控制esp8266智能硬件            | https://github.com/xuhongv/WeChatMiniEsp8266                | 2018.11    |
| 微信公众号airkiss配网以及近场发现在esp8266的实现         | https://github.com/xuhongv/xLibEsp8266Rtos3.1AirKiss        | 2019.3     |
| 微信公众号airkiss配网以及近场发现在esp32/esp32S2的实现   | https://github.com/xuhongv/xLibEsp32IdfAirKiss              | 2019.9     |
| 微信小程序控制esp8266实现七彩效果项目源码                | https://github.com/xuhongv/WCMiniColorSetForEsp8266         | 2019.9     |
| 微信小程序蓝牙配网blufi实现在esp32源码                   | https://github.com/xuhongv/BlufiEsp32WeChat                 | 2019.11    |
| 微信小程序蓝牙ble控制esp32七彩灯效果                     | https://blog.csdn.net/xh870189248/article/details/101849759 | 2019.10    |
| 可商用的事件分发的微信小程序mqtt断线重连框架             | https://blog.csdn.net/xh870189248/article/details/88718302  | 2019.2     |
| 微信小程序以 websocket 连接阿里云IOT物联网平台mqtt服务器 | https://blog.csdn.net/xh870189248/article/details/91490697  | 2019.6     |
| 微信公众号网页实现连接mqtt服务器                         | https://blog.csdn.net/xh870189248/article/details/100738444 | 2019.9     |
| 微信小程序AP配网Wi-Fi ESP32-S2模块                       | https://github.com/xuhongv/WeChatMiniAP2Net                 | 2020.9.21  |
| 云云对接方案服务器源码 xClouds-php PHP TP5开发框架       | https://github.com/xuhongv/xClouds-php                      | 2020.8.4   |
| 微信小程序端自定义view七彩采集颜色圆环控件               | https://github.com/xuhongv/WCMiniColorPicker                | 2019.12.04 |