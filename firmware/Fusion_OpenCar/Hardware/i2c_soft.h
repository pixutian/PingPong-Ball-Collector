/*
 * i2c_soft.h
 * -----------------------------------------------------------------------------
 * 软件 IIC（GPIO 模拟），用于驱动板载 IMU。只实现主机模式下的
 * 寄存器读写，够用且不依赖硬件 I2C 外设。
 */

#ifndef I2C_SOFT_H
#define I2C_SOFT_H

#include "stm32f4xx.h"

#define I2C_OK        0
#define I2C_ERR_NACK  1

/* 初始化两根信号线，重复调用无副作用 */
void i2c_init(void);

/* 写寄存器：addr7 为 7 位从机地址，reg 为寄存器号，buf/len 为数据 */
uint8_t i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *buf, uint16_t len, uint32_t timeout);

/* 读寄存器：连续读 len 字节 */
uint8_t i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len, uint32_t timeout);

#endif /* I2C_SOFT_H */
