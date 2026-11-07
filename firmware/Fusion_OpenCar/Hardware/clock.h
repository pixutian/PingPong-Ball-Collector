/*
 * clock.h
 * -----------------------------------------------------------------------------
 * 系统时基：SysTick 产生固定频率的毫秒节拍，同时提供阻塞式延时。
 */

#ifndef CLOCK_H
#define CLOCK_H

#include "stm32f4xx.h"

/* 启动节拍，freq_hz 为每秒中断次数（本工程用 1000，即 1ms 一次） */
void tick_init(uint32_t freq_hz);

/* 读取上电以来的毫秒计数值 */
uint32_t tick_ms(void);

/* 阻塞延时，基于上面的毫秒计数值 */
void delay_ms(uint32_t ms);

/* 阻塞延时，基于 SysTick 计数器，精度到微秒 */
void delay_us(uint32_t us);

#endif /* CLOCK_H */
