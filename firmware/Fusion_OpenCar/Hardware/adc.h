/*
 * adc.h
 * -----------------------------------------------------------------------------
 * 电池电压采样：ADC1 单通道、单次转换、软件触发。
 */

#ifndef ADC_H
#define ADC_H

#include "stm32f4xx.h"

/* 初始化采样通道（PB0 与 PB1 一并配置为模拟输入） */
void bat_adc_init(void);

/* 读取一次 12 位原始值 */
uint16_t bat_adc_raw(void);

/* 读取一次折算后的电池电压，单位 V */
float bat_adc_voltage(void);

#endif /* ADC_H */
