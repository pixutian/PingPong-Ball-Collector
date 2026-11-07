/*
 * app_chassis.h
 * -----------------------------------------------------------------------------
 * 底盘控制层：速度档位、四轮目标转速、速度环 PI、电机输出。
 *
 * 差速底盘约定：左列 A/C 同速，右列 B/D 同速，右侧编码器读回来取负号，
 * 因此 PI 看到的四路转速都是"同向为正"，目标值可以直接同相给定。
 */

#ifndef APP_CHASSIS_H
#define APP_CHASSIS_H

#include "stm32f4xx.h"

typedef enum {
    WHEEL_A = 0,
    WHEEL_B,
    WHEEL_C,
    WHEEL_D,
    WHEEL_NUM
} chassis_wheel_t;

/* 档位复位到中间档（上电时调用） */
void chassis_init(void);

/* 档位加减一档，到顶 / 到底再操作会回到中间档 */
void chassis_gear_change(int8_t direction);

/* 当前档位与当前目标转速 */
uint8_t chassis_gear(void);
int chassis_speed_set(void);

/* 方向指令（0~8 为摇杆方向，20/21 为叶片电机档位键，其余视为松开） */
void chassis_apply_command(uint8_t dir_key);

/* ------------------------ 叶片（分机）电机 7 档 ------------------------
 * 与底盘相同的循环规则：档位只存序号，占空比由序号换算，
 * +1 升档、-1 降档，到顶 / 到底再操作回到中间档。
 * 0~6 档对应占空比 40% / 50% / 60% / 70% / 80% / 90% / 100%。 */
void    aux_gear_change(int8_t direction);   /* 升 / 降一档 */
uint8_t aux_gear(void);                      /* 当前档位 0~6 */
uint8_t aux_gear_duty(void);                 /* 当前档位对应的占空比 % */

/* 速度环：读编码器 -> 算转速 -> PI -> 输出 PWM，5ms 调用一次 */
void chassis_control_step(void);

/* 查询某轮的目标 / 实际转速，单位 rpm */
float chassis_target_rpm(chassis_wheel_t wheel);
float chassis_actual_rpm(chassis_wheel_t wheel);

#endif /* APP_CHASSIS_H */
