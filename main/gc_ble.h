/*
 * @Descripttion : 
 * @version      : 
 * @Author       : Kevincoooool
 * @Date         : 2020-09-16 20:32:49
 * @LastEditors  : Kevincoooool
 * @LastEditTime : 2020-12-06 17:55:56
 * @FilePath     : \esp-adf_new\tcp_server_client\main\huafuu_ble.h
 */

#ifndef __CHIPINTELLI__BLE__H
#define __CHIPINTELLI__BLE__H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum
{
    IDX_SVC,
    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    HRS_IDX_NB,
};
typedef struct
{
    uint8_t len;
    uint8_t packet[20];
}gatt_msg_t;

void bt_init(void);

void ble_gatts_init(void);

BaseType_t send_msg_to_app(gatt_msg_t *gatt_msg);
uint8_t gatt_send(uint8_t *val,uint16_t len);
void set_ble_bypass_mode_false(void);
void bt_task(void * parameter);
#endif

