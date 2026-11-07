/*
 * screen.h
 * -----------------------------------------------------------------------------
 * 128x64 单色 OLED（SSD1306，四线 SPI 由 GPIO 模拟）。
 *
 * 显示采用页结构：GRAM 按 [列][页] 组织，最后统一刷新，避免逐点写屏。
 * 字体固定 12 像素高（6x12 点阵，字距 8）。
 */

#ifndef SCREEN_H
#define SCREEN_H

#include "stm32f4xx.h"

/* 初始化屏幕并清屏 */
void screen_init(void);

/* 清空缓存并刷新 */
void screen_clear(void);

/* 把缓存推到屏幕 */
void screen_refresh(void);

/* 显示一个字符（12 像素高） */
void screen_char(uint8_t x, uint8_t y, char ch);

/* 显示字符串，超宽自动换行 */
void screen_text(uint8_t x, uint8_t y, const char *text);

/* 显示定长整数，左侧不足位补空格 */
void screen_number(uint8_t x, uint8_t y, uint32_t number, uint8_t digits);

/* 显示带符号浮点数，int_len 为整数位，dec_len 为小数位（2 或 3） */
void screen_float(uint8_t x, uint8_t y, float value, uint8_t int_len, uint8_t dec_len);

#endif /* SCREEN_H */
