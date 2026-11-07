/*
 * imu_reg.h
 * -----------------------------------------------------------------------------
 * ICM-20948 与片外 AK09916 的寄存器地址表（只列本工程用到的那部分）。
 * 数据手册：ICM-20948 Register Map，Bank0 ~ Bank3 分页访问，换页写 REG_BANK_SEL。
 */

#ifndef IMU_REG_H
#define IMU_REG_H

/* 7 位器件地址 */
#define IMU_ADDR_ICM20948      0x68
#define IMU_ADDR_AK09916       0x0C

/* 分页选择寄存器（四个 Bank 共用同一个地址） */
#define REG_BANK_SEL           0x7F
#define BANK_0                 0x00
#define BANK_1                 0x10
#define BANK_2                 0x20
#define BANK_3                 0x30

/* ---------------- Bank 0 ---------------- */
#define REG_WHO_AM_I           0x00    /* 读回 0xEA 说明芯片正常 */
#define REG_USER_CTRL          0x03
#define REG_PWR_MGMT_1         0x06
#define REG_PWR_MGMT_2         0x07
#define REG_INT_PIN_CFG        0x0F
#define REG_ACCEL_XOUT_H       0x2D    /* 从这里开始连续 12 字节：加速度 + 角速度 */

/* ---------------- Bank 2 ---------------- */
#define REG_GYRO_SMPLRT_DIV    0x00
#define REG_GYRO_CONFIG_1      0x01
#define REG_ACCEL_SMPLRT_DIV_2 0x11
#define REG_ACCEL_CONFIG       0x14

/* ---------------- 磁力计 AK09916 ---------------- */
#define REG_MAG_WIA            0x01    /* 读回 0x09 */
#define REG_MAG_ST1            0x10
#define REG_MAG_HXL            0x11    /* 从 HXL 起连续 6 字节为三轴数据，0x18 是 ST2 */
#define REG_MAG_ST2            0x18
#define REG_MAG_CNTL2          0x31

/* ---------------- 常用配置值 ---------------- */
#define ICM_WHO_AM_I_VALUE     0xEA
#define AK_WHO_AM_I_VALUE      0x09

#define PWR_MGMT_1_RESET       0x80    /* bit7：软复位 */
#define PWR_MGMT_1_AUTO_CLK    0x01    /* 自动选择最佳时钟源 */
#define USER_CTRL_ALL_OFF      0x00    /* 关 DMP / FIFO / 从机 IIC */
#define INT_PIN_CFG_BYPASS     0x02    /* 旁路模式，主机可直接访问磁力计 */
#define MAG_CNTL2_CONT_100HZ   0x08    /* 连续测量模式 4，100Hz */

/* 陀螺仪：±2000dps、开 DLPF、带宽 11.6Hz */
#define GYRO_CONFIG_1_VALUE    ((3 << 1) | (1 << 0) | (3 << 3))
/* 加速度计：±2g、开 DLPF、带宽 5.7Hz */
#define ACCEL_CONFIG_VALUE     ((0 << 1) | (1 << 0) | (5 << 3))
#define IMU_ODR_DIVIDER        0x04

#endif /* IMU_REG_H */
