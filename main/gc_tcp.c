/* tcp_perf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "gc_ble.h"
#include "gc_tcp.h"
#include "driver/uart.h"
#include "lcd.h"
/* FreeRTOS event group to signal when we are connected to wifi */
EventGroupHandle_t tcp_event_group;

/*socket*/
static int server_socket = 0;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;
static unsigned int socklen = sizeof(client_addr);
static int connect_socket = 0;
bool g_rxtx_need_restart = false;
uint8_t Connect_cnt = 0;
int g_total_data = 0;
extern uint8_t reply_data[4];
struct sockinfo
{
	int sock;
	sa_family_t sa_familyType;
	char *remoteIp;
	u16_t remotePort;
};
uint8_t Txbuffer[8] = {0};
int addr_family;
int ip_protocol;
char addr_str[128];
struct sockinfo remoteInfo[100];
#if EXAMPLE_ESP_TCP_PERF_TX && EXAMPLE_ESP_TCP_DELAY_INFO

int g_total_pack = 0;
int g_send_success = 0;
int g_send_fail = 0;
int g_delay_classify[5] = {0};

#endif /*EXAMPLE_ESP_TCP_PERF_TX && EXAMPLE_ESP_TCP_DELAY_INFO*/

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	switch (event->event_id)
	{
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		esp_wifi_connect();
		xEventGroupClearBits(tcp_event_group, WIFI_CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		ESP_LOGI(TAG, "got ip:%s\n",
				 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
		xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		ESP_LOGI(TAG, "station:" MACSTR " join,AID=%d",
				 MAC2STR(event->event_info.sta_connected.mac),
				 event->event_info.sta_connected.aid);

		xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED: //只要有一个人断开  那就全部重新来
		ESP_LOGI(TAG, "station:" MACSTR "leave,AID=%d\n",
				 MAC2STR(event->event_info.sta_disconnected.mac),
				 event->event_info.sta_disconnected.aid);
		g_rxtx_need_restart = true;

		xEventGroupClearBits(tcp_event_group, WIFI_CONNECTED_BIT);
		break;
	default:
		break;
	}
	return ESP_OK;
}
int receive_sock = 0;
void Receive_One(void *pvParameters)
{
	int len = 0;

	char databuff[1024];

	while (1)
	{
		if (reply_data[3])
		{
			//每次接收都要清空接收数组
			memset(databuff, 0x00, sizeof(databuff));
			len = recv(receive_sock, databuff, sizeof(databuff), 0);
			g_rxtx_need_restart = false;
			if (len > 0)
			{
				g_total_data += len;
				ESP_LOGI(TAG, "\nReceiveData: %x %x %x %x %x %x %x %x", databuff[0], databuff[1], databuff[2], databuff[3], databuff[4], databuff[5], databuff[6], databuff[7]);
				//把从机器虎收到的数据发给APP
				// strcat(dev_id,databuff);
				//send(receive_sock, databuff, len, 0);
				gatt_send((uint8_t *)databuff, len);
			}
			else
			{
				show_socket_error_reason("recv_data", receive_sock);
				g_rxtx_need_restart = true;
#if !TCP_SERVER_CLIENT_OPTION
				break;
#endif
			}
		}
	}

	close_socket();
	//	g_rxtx_need_restart = true;
	vTaskDelete(NULL);
}
//send data
void send_data(void *pvParameters)
{
	int write_len = 0;
	char uart_data[8] = {0xa5, 0xfa, 0X00, 0x81, 0x00, 0x03, 0x23, 0xfb};
	int i = 0;
	ESP_LOGI(TAG, "start sending...");
	// uart_config_t uart_config =
	// 	{
	// 		.baud_rate = 115200,
	// 		.data_bits = UART_DATA_8_BITS,
	// 		.parity = UART_PARITY_DISABLE,
	// 		.stop_bits = UART_STOP_BITS_1,
	// 		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	// 		.rx_flow_ctrl_thresh = 120,
	// 		.use_ref_tick = 0,
	// 	};
	// uart_param_config(UART_NUM_0, &uart_config);
	// uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); //tx   rx   rts cts
	// uart_driver_install(UART_NUM_0, 1024 * 10, 1024 * 10, 20, NULL, 0);

	while (1)
	{
		uint8_t Valid_cnt = 0;
		int Valid_sock[100] = {0};
		for (i = 0; i < Connect_cnt; i++)
		{
			//发送前先检测每个socket是否正常
			if (check_working_socket(remoteInfo[i].sock) || get_socket_error_code(remoteInfo[i].sock) == 5  \
			|| get_socket_error_code(remoteInfo[i].sock) == 110)
			{
				shutdown(remoteInfo[i].sock, 0);
				close(remoteInfo[i].sock);
				remoteInfo[i].sock = 0;
				for (i = 0; i < Connect_cnt; i++)
					printf("%d: remoteInfo sock:%d   \n", i, remoteInfo[i].sock);
				xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
			}
			else if (remoteInfo[i].sock != 0)
			{
				Valid_sock[Valid_cnt] = remoteInfo[i].sock;
				Valid_cnt++;
			}
		}
		reply_data[3] = Valid_cnt;
		receive_sock = Valid_sock[Valid_cnt - 1];
		// Read data from the UART
		// int len = uart_read_bytes(UART_NUM_0, uart_data, 20, 5 / portTICK_RATE_MS);
		if (Valid_cnt && Txbuffer[0] != 0)
		{
			ESP_LOGI(TAG, "已有%d个客户端连接!当前有效客户端列表：", Valid_cnt);
			for (uint8_t i = 0; i < Valid_cnt; i++)
				printf("%d: sock:%d  ", i, Valid_sock[i]);
			ESP_LOGI(TAG, "\nSendData: %x %x %x %x %x %x %x %x", Txbuffer[0], Txbuffer[1], Txbuffer[2], Txbuffer[3], Txbuffer[4], Txbuffer[5], Txbuffer[6], Txbuffer[7]);
			for (i = 0; i < Valid_cnt; i++)
			{
				write_len = send(Valid_sock[i], Txbuffer, 8, 0);
				if (write_len > 0)
				{
					g_total_data += write_len;
				}
				else
				{
					int err = get_socket_error_code(Valid_sock[i]);
					if (err != ENOMEM)
					{
						show_socket_error_reason("send_data", Valid_sock[i]); //不存在此sock  需要从列表中去掉
						xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
						break;
					}
				}
			}
			memset(Txbuffer, 0, 8);
		}
		vTaskDelay(100);
	}
	g_rxtx_need_restart = true;

	vTaskDelete(NULL);
}

//receive data
void recv_data(void *pvParameters)
{
	int len = 0;

	char databuff[1024];

	while (1)
	{
		//每次接收都要清空接收数组
		memset(databuff, 0x00, sizeof(databuff));
		len = recv(connect_socket, databuff, sizeof(databuff), 0);
		g_rxtx_need_restart = false;
		if (len > 0)
		{
			g_total_data += len;
			//打印接收到的数组
			ESP_LOGI(TAG, "recvData: %s\n", databuff);
			//原路返回，不指定某个客户端
			send(connect_socket, databuff, sizeof(databuff), 0);
			//sendto(connect_socket, databuff , sizeof(databuff), 0, (struct sockaddr *) &remote_addr,sizeof(remote_addr));
		}
		else
		{
			show_socket_error_reason("recv_data", connect_socket);
			g_rxtx_need_restart = true;
#if !TCP_SERVER_CLIENT_OPTION
			break;
#endif
		}
	}

	close_socket();
	g_rxtx_need_restart = true;
	vTaskDelete(NULL);
}
TaskHandle_t Send_Handle = NULL;
esp_err_t create_tcp_server(bool isCreatServer)
{
	static bool is_crate = true;

	if (is_crate)
	{
		if (pdPASS != xTaskCreate(&send_data, "send_data", 4096, NULL, 4, &Send_Handle))
		{
			ESP_LOGI(TAG, "send task create fail!");
		}
		else
		{
			ESP_LOGI(TAG, "send task create succeed!");
		}

		is_crate = false;
		ESP_LOGI(TAG, "server socket....,port=%d", TCP_PORT);
		server_socket = socket(AF_INET, SOCK_STREAM, 0);

		if (server_socket < 0)
		{
			show_socket_error_reason("create_server", server_socket);
			return ESP_FAIL;
		}

		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(TCP_PORT);
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
		{
			show_socket_error_reason("bind_server", server_socket);
			close(server_socket);
			return ESP_FAIL;
		}
	}

	if (listen(server_socket, 50) < 0)
	{
		show_socket_error_reason("listen_server", server_socket);
		close(server_socket);
		is_crate = true;
		return ESP_FAIL;
	}

	connect_socket = accept(server_socket, (struct sockaddr *)&client_addr, &socklen);
	eTaskState TaskState = eTaskGetState(&Send_Handle); //获取query_task任务的任务状态
	ESP_LOGI(TAG, "TaskState:%d", TaskState);
	if (TaskState != eReady) //如果正在挂起的控制才恢复
	{
		if (pdPASS != xTaskCreate(&send_data, "send_data", 4096, NULL, 4, &Send_Handle))
		{
			ESP_LOGI(TAG, "send task create fail!");
		}
		else
		{
			ESP_LOGI(TAG, "send task create succeed!");
		}
	}

	remoteInfo[Connect_cnt].sock = connect_socket;
	if (client_addr.sin_family == PF_INET)
	{
		remoteInfo[Connect_cnt].remoteIp = inet_ntoa_r(((struct sockaddr_in *)&client_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
		remoteInfo[Connect_cnt].sa_familyType = PF_INET;
	}
	remoteInfo[Connect_cnt].remotePort = ntohs(client_addr.sin_port);

	ESP_LOGI(TAG, "Currently socket NO:%d IP is:%s PORT is:%d", connect_socket, remoteInfo[Connect_cnt].remoteIp, remoteInfo[Connect_cnt].remotePort);
	ESP_LOGI(TAG, "station:%d", ((struct sockaddr_in *)&client_addr)->sin_addr.s_addr);

	ESP_LOGI(TAG, "新的socket号=%d", remoteInfo[Connect_cnt].sock);
	if (remoteInfo[Connect_cnt].sock < 0)
	{
		show_socket_error_reason("accept_server", remoteInfo[Connect_cnt].sock);
		close(server_socket);
		return ESP_FAIL;
	}
	int keepAlive = 1;		 // 开启keepalive属性
	int keepIdle = 20000000; // 如该连接在60秒内没有任何数据往来,则进行探测
	int keepInterval = 5;	 // 探测时发包的时间间隔为5 秒
	int keepCount = 3;		 // 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发.

	setsockopt(remoteInfo[Connect_cnt].sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
	setsockopt(remoteInfo[Connect_cnt].sock, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
	setsockopt(remoteInfo[Connect_cnt].sock, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
	setsockopt(remoteInfo[Connect_cnt].sock, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

	ESP_LOGI(TAG, "新的客户端连接已建立!");
	Connect_cnt++;
	ESP_LOGI(TAG, "已有%d个客户端连接过!列表：", Connect_cnt);
	for (uint8_t i = 0; i < Connect_cnt; i++)
		printf("%d: sock:%d  ip: %s \n", i, remoteInfo[i].sock, remoteInfo[i].remoteIp);

	if (pdPASS != xTaskCreate(&Receive_One, "Receive_One", 4096, NULL, 4, &Send_Handle))
	{
		ESP_LOGI(TAG, "Receive_One task create fail!");
	}
	else
	{
		ESP_LOGI(TAG, "Receive_One task create succeed!");
	}
	return ESP_OK;
}

//创建TCP客户端连接到指定的服务器
esp_err_t create_tcp_client()
{

	ESP_LOGI(TAG, "will connect gateway ssid : %s port:%d\n",
			 TCP_SERVER_ADRESS, TCP_PORT);

	connect_socket = socket(AF_INET, SOCK_STREAM, 0);

	if (connect_socket < 0)
	{
		show_socket_error_reason("create client", connect_socket);
		return ESP_FAIL;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(TCP_PORT);
	server_addr.sin_addr.s_addr = inet_addr(TCP_SERVER_ADRESS);
	ESP_LOGI(TAG, "connectting server...");
	if (connect(connect_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		show_socket_error_reason("client connect", connect_socket);
		ESP_LOGE(TAG, "connect failed!");
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "connect success!");
	return ESP_OK;
}

//wifi_init_sta
void wifi_init_sta()
{
	tcp_event_group = xEventGroupCreate();

	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	wifi_config_t wifi_config = {
		.sta = {
			.ssid = GATEWAY_SSID,
			.password = GATEWAY_PAS},
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "connect to ap SSID:%s password:%s \n",
			 GATEWAY_SSID, GATEWAY_PAS);
}

//wifi_init_softap
void wifi_init_softap()
{
	tcp_event_group = xEventGroupCreate();

	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	wifi_config_t wifi_config = {
		.ap = {
			.ssid = SOFT_AP_SSID,
			.ssid_len = 0,
			.max_connection = SOFT_AP_MAX_CONNECT,
			.password = SOFT_AP_PAS,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK},
	};
	if (strlen(EXAMPLE_DEFAULT_PWD) == 0)
	{
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "SoftAP set finish:%s pas:%s \n",
			 EXAMPLE_DEFAULT_SSID, EXAMPLE_DEFAULT_PWD);
}

int get_socket_error_code(int socket)
{
	int result;
	u32_t optlen = sizeof(int);
	int err = getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen);
	if (err == -1)
	{
		//		ESP_LOGE(TAG, "getsockopt failed:%s", strerror(err));
		return -1;
	}
	return result;
}

int show_socket_error_reason(const char *str, int socket)
{
	int err = get_socket_error_code(socket);

	if (err != 0)
	{
		ESP_LOGW(TAG, "%s  %s socket error %d %s", __func__, str, err, strerror(err));
	}

	return err;
}

int check_working_socket(int sock_num)
{
	int ret;

	ESP_LOGD(TAG, "check server_socket");
	ret = get_socket_error_code(sock_num);
	if (ret != 0)
	{
		//		ESP_LOGW(TAG, "server socket error %d %s", ret, strerror(ret));
	}
	if (ret == ECONNRESET)
	{
		return ret;
	}

	return 0;
}

void close_socket()
{
	close(connect_socket);
	close(server_socket);
}
