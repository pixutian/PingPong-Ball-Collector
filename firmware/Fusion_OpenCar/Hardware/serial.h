/*
 * serial.h
 * -----------------------------------------------------------------------------
 * 串口层：调试串口（USART1，只发送）与蓝牙串口（USART2，收发 + 接收中断）。
 * USART2 的中断入口放在 app_link.c，由协议层自己接管接收字节。
 */

#ifndef SERIAL_H
#define SERIAL_H

#include "stm32f4xx.h"

/* 调试串口，只用于打印 */
void debug_uart_init(uint32_t baud);

/* 蓝牙串口，波特率需与手机端一致 */
void link_uart_init(uint32_t baud);

/* 阻塞发送，直到最后一个字节送出去；text 以 '\0' 结尾 */
void link_uart_send(const char *text);

#endif /* SERIAL_H */
