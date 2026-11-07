/*
 * imu.c
 * -----------------------------------------------------------------------------
 * ICM-20948 驱动与姿态解算。
 *
 * 初始化流程（与数据手册的应用手册顺序一致）：
 *   选 Bank0 -> 读 WHO_AM_I -> 软复位 -> 关 DMP/FIFO -> 自动时钟 -> 选 Bank2
 *   -> 配置陀螺仪与加速度计的采样率、量程、滤波器 -> 回 Bank0
 *   -> 打开磁力计旁路 -> 校验 AK09916 -> 磁力计开连续测量。
 *
 * 姿态解算：把加速度计测到的重力方向作为观测量，用 PI 修正陀螺仪积分出的
 * 四元数，抑制纯积分带来的漂移。
 */

#include "imu.h"
#include "imu_reg.h"
#include "i2c_soft.h"
#include "clock.h"

#include <math.h>
#include <string.h>

/* 量程与分辨率换算系数，见数据手册 Sensitivity 表 */
#define ACCEL_SCALE_PER_LSB    0.00059814453125f    /* ±2g 档，m/s^2/LSB */
#define GYRO_SCALE_DPS_PER_LSB 0.06103515625f       /* ±2000dps 档，dps/LSB */
#define DEG_TO_RAD             0.01745329252f
#define RAD_TO_DEG             57.29578f
#define MAG_SCALE_PER_LSB      0.1495361328125f     /* ±4900uT 档，uT/LSB */

#define ATTITUDE_KP            0.8f
#define ATTITUDE_KI            0.0001f
#define ATTITUDE_DT            0.005f               /* 5ms，与 200Hz 采样周期一致 */

/* ------------------------------------------------------------------ 工具 */

static void write_reg8(uint8_t reg, uint8_t value, uint32_t timeout)
{
    (void)i2c_write_reg(IMU_ADDR_ICM20948, reg, &value, 1, timeout);
}

static uint8_t read_reg8(uint8_t reg)
{
    uint8_t value = 0;

    (void)i2c_read_reg(IMU_ADDR_ICM20948, reg, &value, 1, 100);

    return value;
}

/* ------------------------------------------------------------------ 初始化 */

uint8_t imu_init(void)
{
    uint8_t checksum = 0;
    uint8_t value;
    uint8_t who;
    uint8_t mag_mode = MAG_CNTL2_CONT_100HZ;

    i2c_init();

    /* 选 Bank0，并核对芯片型号 */
    write_reg8(REG_BANK_SEL, BANK_0, 500);
    who = read_reg8(REG_WHO_AM_I);
    if (who != ICM_WHO_AM_I_VALUE) {
        return 1;
    }

    /* 软复位后等 100ms 让内部寄存器恢复默认值 */
    write_reg8(REG_PWR_MGMT_1, PWR_MGMT_1_RESET, 500);
    delay_ms(100);

    write_reg8(REG_USER_CTRL, USER_CTRL_ALL_OFF, 500);

    /* 退出低功耗，打开温度传感器，时钟源自动选择 */
    write_reg8(REG_PWR_MGMT_1, PWR_MGMT_1_AUTO_CLK, 100);

    /* 配置陀螺仪与加速度计：采样率 1.1kHz / (1+div)，开 DLPF */
    write_reg8(REG_BANK_SEL, BANK_2, 100);
    write_reg8(REG_GYRO_SMPLRT_DIV, IMU_ODR_DIVIDER, 100);
    write_reg8(REG_GYRO_CONFIG_1, GYRO_CONFIG_1_VALUE, 100);
    write_reg8(REG_ACCEL_SMPLRT_DIV_2, IMU_ODR_DIVIDER, 100);
    write_reg8(REG_ACCEL_CONFIG, ACCEL_CONFIG_VALUE, 100);
    write_reg8(REG_BANK_SEL, BANK_0, 100);

    /* 打开旁路，让主机能直接访问磁力计 */
    write_reg8(REG_INT_PIN_CFG, INT_PIN_CFG_BYPASS, 100);

    checksum += i2c_read_reg(IMU_ADDR_AK09916, REG_MAG_WIA, &value, 1, 100);
    if (value != AK_WHO_AM_I_VALUE) {
        return 1;
    }

    checksum += i2c_write_reg(IMU_ADDR_AK09916, REG_MAG_CNTL2, &mag_mode, 1, 100);

    return (checksum != 0) ? 1 : 0;
}

/* ------------------------------------------------------------------ 取数 */

void imu_read_sample(imu_sample_t *sample)
{
    uint8_t raw[12];
    uint8_t mag_raw[8];
    int16_t accel[3];
    int16_t gyro[3];

    /* 加速度 + 角速度连续 12 字节 */
    (void)i2c_read_reg(IMU_ADDR_ICM20948, REG_ACCEL_XOUT_H, raw, 12, 100);

    accel[0] = (int16_t)((raw[0] << 8) | raw[1]);
    accel[1] = (int16_t)((raw[2] << 8) | raw[3]);
    accel[2] = (int16_t)((raw[4] << 8) | raw[5]);

    gyro[0] = (int16_t)((raw[6] << 8)  | raw[7]);
    gyro[1] = (int16_t)((raw[8] << 8)  | raw[9]);
    gyro[2] = (int16_t)((raw[10] << 8) | raw[11]);

    sample->accel.x = (float)accel[0] * ACCEL_SCALE_PER_LSB;
    sample->accel.y = (float)accel[1] * ACCEL_SCALE_PER_LSB;
    sample->accel.z = (float)accel[2] * ACCEL_SCALE_PER_LSB;

    sample->gyro.x = (float)gyro[0] * GYRO_SCALE_DPS_PER_LSB * DEG_TO_RAD;
    sample->gyro.y = (float)gyro[1] * GYRO_SCALE_DPS_PER_LSB * DEG_TO_RAD;
    sample->gyro.z = (float)gyro[2] * GYRO_SCALE_DPS_PER_LSB * DEG_TO_RAD;

    /* 静止时角速度的零漂直接压成 0，避免航向缓慢漂移 */
    if (fabsf(sample->gyro.z) < 0.05f) {
        sample->gyro.z = 0.0f;
    }

    /* 磁力计：HXL 起 8 字节，最后一个字节是 ST2，bit3 必须为 0 才说明数据已就绪 */
    (void)i2c_read_reg(IMU_ADDR_AK09916, REG_MAG_HXL, mag_raw, 8, 100);

    if (((mag_raw[7] >> 3) & 0x01) == 0) {
        sample->mag.x = (float)(int16_t)((mag_raw[1] << 8) | mag_raw[0]) * MAG_SCALE_PER_LSB;
        sample->mag.y = (float)(int16_t)((mag_raw[3] << 8) | mag_raw[2]) * MAG_SCALE_PER_LSB;
        sample->mag.z = (float)(int16_t)((mag_raw[5] << 8) | mag_raw[4]) * MAG_SCALE_PER_LSB;
    }
    /* 数据不可靠时保留上一次的值 */
}

/* ------------------------------------------------------------------ 姿态 */

/* 快速求平方根的倒数（牛顿迭代一次），省掉一次 sqrtf 的除法开销 */
static float inv_sqrt(float number)
{
    long  i;
    float x2;
    float y;

    x2 = number * 0.5f;
    y  = number;
    i  = *(long *)&y;
    i  = 0x5F3759DF - (i >> 1);
    y  = *(float *)&i;
    y  = y * (1.5f - (x2 * y * y));

    return y;
}

void imu_update_attitude(const imu_sample_t *sample, imu_attitude_t *attitude)
{
    static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;   /* 四元数 */
    static float err_int[3] = {0.0f, 0.0f, 0.0f};              /* 误差积分 */

    float half_dt = ATTITUDE_DT * 0.5f;
    float norm;
    float vx, vy, vz;
    float ex, ey, ez;

    float q0q0 = q0 * q0, q0q1 = q0 * q1, q0q2 = q0 * q2, q0q3 = q0 * q3;
    float q1q1 = q1 * q1, q1q2 = q1 * q2, q1q3 = q1 * q3;
    float q2q2 = q2 * q2, q2q3 = q2 * q3, q3q3 = q3 * q3;

    float gx = sample->gyro.x;
    float gy = sample->gyro.y;
    float gz = sample->gyro.z;

    float ax = sample->accel.x;
    float ay = sample->accel.y;
    float az = sample->accel.z;

    /* 由四元数推出的重力方向（旋转矩阵第三行） */
    vx = 2.0f * (q1q3 - q0q2);
    vy = 2.0f * (q0q1 + q2q3);
    vz = 1.0f - 2.0f * (q1q1 + q2q2);

    /* 加速度计归一化 */
    norm = inv_sqrt(ax * ax + ay * ay + az * az);
    ax *= norm;
    ay *= norm;
    az *= norm;

    /* 观测值与推算值的叉积就是修正量 */
    ex = ay * vz - az * vy;
    ey = az * vx - ax * vz;
    ez = ax * vy - ay * vx;

    err_int[0] += ex;
    err_int[1] += ey;
    err_int[2] += ez;

    gx += ATTITUDE_KP * ex + ATTITUDE_KI * err_int[0];
    gy += ATTITUDE_KP * ey + ATTITUDE_KI * err_int[1];
    gz += ATTITUDE_KP * ez + ATTITUDE_KI * err_int[2];

    /* 一阶龙格库塔积分四元数（用旧值算出增量，再统一加上） */
    {
        float dq0 = (-q1 * gx - q2 * gy - q3 * gz) * half_dt;
        float dq1 = ( q0 * gx + q2 * gz - q3 * gy) * half_dt;
        float dq2 = ( q0 * gy - q1 * gz + q3 * gx) * half_dt;
        float dq3 = ( q0 * gz + q1 * gy - q2 * gx) * half_dt;

        q0 += dq0;
        q1 += dq1;
        q2 += dq2;
        q3 += dq3;
    }

    /* 归一化，防止数值漂移 */
    norm = inv_sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= norm;
    q1 *= norm;
    q2 *= norm;
    q3 *= norm;

    /* 四元数转欧拉角 */
    attitude->roll  = atan2f(2.0f * q2 * q3 + 2.0f * q0 * q1,
                             -2.0f * q1 * q1 - 2.0f * q2 * q2 + 1.0f);
    attitude->pitch = asinf(-2.0f * q1 * q3 + 2.0f * q0 * q2);
    attitude->yaw   = atan2f(2.0f * (q1 * q2 + q0 * q3),
                             q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3);

    attitude->roll  *= RAD_TO_DEG;
    attitude->pitch *= RAD_TO_DEG;
    attitude->yaw   *= RAD_TO_DEG;
}
