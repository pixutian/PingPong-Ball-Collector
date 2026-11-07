/*
 * app_link.h
 * -----------------------------------------------------------------------------
 * 上位机（手机 APP）通讯层：解析蓝牙串口收到的指令帧，并把状态回传给 APP。
 *
 * 帧格式：{ ... }$ ，其中第一个字符区分用途：
 *   {A...} 页面一：四轮转速、电量、航向
 *   {B...} 页面二：俯仰 / 横滚 / 航向
 *   {C...} 调试参数
 * 下行指令：
 *   'I' 'J' 'K'   切换 APP 页面
 *   'A'~'H'       摇杆方向（对应 1~8）
 *   'X' / 'Y'     升档 / 降档
 *   'T' / 'U'     分机电机加减占空比
 *   {Q:P}         请求回传调试参数
 *   {Q:W}         请求保存参数
 *   {0:xxx} 等    设置调试参数
 */

#ifndef APP_LINK_H
#define APP_LINK_H

#include "stm32f4xx.h"

/* 摇杆方向，0 表示松开；20/21 是分机电机按键；24/25 是档位按键 */
extern volatile uint8_t g_link_dir;

/* 当前 APP 页面编号 0~2 */
extern volatile uint8_t g_link_page;

/* APP 请求标志，主循环处理完自行清零 */
extern volatile uint8_t g_link_save_request;
extern volatile uint8_t g_link_report_request;

/* 两个可在线修改的调试参数，会显示在屏幕上并回传给 APP */
extern uint16_t g_link_param1;
extern uint16_t g_link_param2;

/* 打开蓝牙串口，开始接收 */
void link_init(uint32_t baud);

/* 发送一帧文本 */
void link_send_text(const char *text);

#endif /* APP_LINK_H */
