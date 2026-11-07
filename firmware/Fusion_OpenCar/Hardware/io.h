/*
 * io.h
 * -----------------------------------------------------------------------------
 * 板载指示与输入输出：状态灯、蜂鸣器、用户按键、急停开关。
 */

#ifndef IO_H
#define IO_H

#include "stm32f4xx.h"

/* 一次性初始化四路普通 IO */
void io_init(void);

/* 状态灯 */
void led_write(uint8_t on);
void led_toggle(void);

/* 蜂鸣器 */
void buzzer_write(uint8_t on);

/* 按键，按下返回 1（内部上拉，低电平有效） */
uint8_t key_user_pressed(void);
uint8_t key_estop_pressed(void);

#endif /* IO_H */
