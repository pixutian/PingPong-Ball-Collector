/*
 * imu.h
 * -----------------------------------------------------------------------------
 * 九轴姿态传感器：ICM-20948（三轴加速度 + 三轴陀螺 + 片外 AK09916 三轴磁力计）。
 *
 * 驱动只负责"取数"和"算姿态"两件事，姿态解算采用加速度计 + 陀螺仪的
 * 互补滤波（PI 修正四元数），磁力计数据供后续扩展航向参考。
 */

#ifndef IMU_H
#define IMU_H

#include "stm32f4xx.h"

typedef struct {
    float x;
    float y;
    float z;
} imu_vec3_t;

typedef struct {
    imu_vec3_t accel;   /* 加速度，单位 m/s^2 */
    imu_vec3_t gyro;    /* 角速度，单位 rad/s */
    imu_vec3_t mag;     /* 磁感应强度，单位 uT */
} imu_sample_t;

typedef struct {
    float roll;
    float pitch;
    float yaw;
} imu_attitude_t;

/* 初始化，返回 0 成功，1 失败 */
uint8_t imu_init(void);

/* 读取一组九轴数据 */
void imu_read_sample(imu_sample_t *sample);

/* 用一组九轴数据更新姿态角（度） */
void imu_update_attitude(const imu_sample_t *sample, imu_attitude_t *attitude);

#endif /* IMU_H */
