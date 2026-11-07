/*
 * encoder.h
 * -----------------------------------------------------------------------------
 * 编码器接口：把四个定时器配置成 AB 两相四倍频计数模式，并提供带符号的
 * 转速原始值读取（读完自动清零，因此值本身就是"本周期内的脉冲数"）。
 */

#ifndef ENCODER_H
#define ENCODER_H

#include "stm32f4xx.h"

typedef enum {
    ENC_WHEEL_A = 0,
    ENC_WHEEL_B,
    ENC_WHEEL_C,
    ENC_WHEEL_D
} encoder_wheel_t;

/* 四个编码器统一初始化 */
void encoder_init(void);

/* 读取本周期脉冲数，带符号；读后计数器归零 */
int encoder_read(encoder_wheel_t wheel);

#endif /* ENCODER_H */
