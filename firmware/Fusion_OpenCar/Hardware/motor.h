/*
 * motor.h
 * -----------------------------------------------------------------------------
 * 直流电机与舵机的 PWM 输出层。
 *
 * 四路电机各用两组互补通道驱动 H 桥：正向通道满占空比、反向通道按占空比
 * 递减，等价于给电机一个带符号的电压。传入的 duty 正负号即转向。
 */

#ifndef MOTOR_H
#define MOTOR_H

#include "stm32f4xx.h"

/* 四路电机 PWM + 舵机 PWM 初始化 */
void motor_init(void);

/* duty 范围 -AR R ~ +ARR，正为正转，负为反转 */
void motor_set_duty(int duty_a, int duty_b, int duty_c, int duty_d);

/* 舵机角度，单位度，超出量程自动钳位 */
void servo_set_angle(float degree);

/* 叶片（分机）电机：单路 TB6612，独立于底盘
 *   aux_motor_init()      上电按 AUX_MOTOR_DEFAULT_DUTY 起转
 *   aux_motor_set_duty()  运行中改转速，参数是占空比百分比 0~100
 *   aux_motor_get_duty()  读回当前占空比，屏幕上的 Y 值就是它 */
void aux_motor_init(void);
void aux_motor_set_duty(uint8_t percent);
uint8_t aux_motor_get_duty(void);

#endif /* MOTOR_H */
