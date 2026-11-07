/*
 * app_main.c
 * -----------------------------------------------------------------------------
 * 程序入口：初始化各硬件 -> 启动 200Hz 控制节拍 -> 主循环做显示与通讯。
 *
 * 任务划分：
 *   TIM7 中断(200Hz)  读 IMU、算姿态、读摇杆指令、读编码器、跑速度环、输出 PWM
 *   主循环(约 100Hz)  采样电池、刷新屏幕、把状态打包发给手机 APP
 *   USART2 中断       只把收到的字节交给协议层解析
 */

#include "app_config.h"
#include "app_chassis.h"
#include "app_link.h"

#include "board.h"
#include "clock.h"
#include "io.h"
#include "screen.h"
#include "motor.h"
#include "encoder.h"
#include "adc.h"
#include "serial.h"
#include "imu.h"
#include "i2c_soft.h"

#include <stdio.h>
#include <string.h>

#define CONTROL_TIMER_PERIOD   4999    /* 1MHz 计数下 5000 个计数值 = 5ms */
#define CONTROL_TIMER_PRESCALER 83     /* 84MHz / 84 = 1MHz */
#define CONTROL_TIMER_PRIORITY 6       /* 低于串口，允许被接收中断打断 */

#define ADC_SAMPLE_DIVIDER     10      /* 每 10 个控制周期采一次电池，即 20ms */
#define BUZZER_TICKS           10      /* 保存参数后的提示音长度 */

static volatile uint8_t  s_adc_request = 0;
static volatile uint8_t  s_buzzer_ticks = 0;
static float             s_battery_voltage = 0.0f;

static imu_sample_t      s_imu_data;
static imu_attitude_t    s_imu_attitude;

static char              s_frame_a[256];
static char              s_frame_b[256];
static char              s_frame_c[256];

/* ------------------------------------------------------------ 控制节拍 */

static void control_timer_init(void)
{
    TIM_TimeBaseInitTypeDef base;
    NVIC_InitTypeDef        nvic;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);

    base.TIM_Prescaler     = CONTROL_TIMER_PRESCALER;
    base.TIM_Period        = CONTROL_TIMER_PERIOD;
    base.TIM_ClockDivision = TIM_CKD_DIV1;
    base.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM7, &base);

    nvic.NVIC_IRQChannel                   = TIM7_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = CONTROL_TIMER_PRIORITY;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    TIM_ITConfig(TIM7, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM7, ENABLE);
}

void TIM7_IRQHandler(void)
{
    static uint32_t led_count = 0;
    static uint32_t adc_count = 0;

    if (TIM_GetITStatus(TIM7, TIM_IT_Update) == RESET) {
        return;
    }

    TIM_ClearITPendingBit(TIM7, TIM_IT_Update);

    /* 1. 姿态：读九轴 + 互补滤波 */
    imu_read_sample(&s_imu_data);
    imu_update_attitude(&s_imu_data, &s_imu_attitude);

    /* 2. 指示灯，1 秒翻转一次 */
    led_count++;
    if (led_count > CONTROL_RATE_HZ) {
        led_count = 0;
        led_toggle();
    }

    /* 3. 电池采样请求，20Hz */
    adc_count++;
    if (adc_count >= ADC_SAMPLE_DIVIDER) {
        adc_count = 0;
        s_adc_request = 1;
    }

    /* 4. 按当前摇杆指令更新四轮目标转速 */
    chassis_apply_command(g_link_dir);

    /* 5. 舵机保持中位：四轮差速靠两侧转速差转向，不依赖舵机 */
    servo_set_angle(SERVO_NEUTRAL_DEG);

    /* 6. 收到保存指令就短鸣一声 */
    if (g_link_save_request == 1) {
        g_link_save_request = 0;
        s_buzzer_ticks = BUZZER_TICKS;
    }

    /* 7. 速度环：读编码器 -> PI -> PWM */
    chassis_control_step();
}

/* ------------------------------------------------------------ 显示刷新 */

static void display_update(void)
{
    screen_text(0, 0, "A");
    screen_text(0, 10, "B");
    screen_text(0, 20, "Servo");

    screen_float(15, 0, chassis_target_rpm(WHEEL_A), 3, 2);
    screen_float(15, 10, chassis_target_rpm(WHEEL_B), 3, 2);
    screen_float(70, 0, chassis_actual_rpm(WHEEL_A), 3, 2);
    screen_float(70, 10, chassis_actual_rpm(WHEEL_B), 3, 2);
    screen_float(70, 20, SERVO_NEUTRAL_DEG, 3, 2);

    /* 档位行：目标转速 + 当前档位号，改档后两者应同时跳变 */
    screen_text(0, 30, "S");
    screen_number(15, 30, (uint32_t)chassis_speed_set(), 3);
    screen_text(38, 30, "G");
    screen_number(46, 30, (uint32_t)chassis_gear(), 1);

    /* 叶片电机：Y = 当前占空比%，右侧 G = 该电机的档位号 0~6 */
    screen_text(60, 30, "Y");
    screen_number(80, 30, (uint32_t)aux_motor_get_duty(), 3);
    screen_text(100, 30, "G");
    screen_number(108, 30, (uint32_t)aux_gear(), 1);

    /* APP 可调的调试参数 */
    screen_number(0, 40, g_link_param1, 5);
    screen_number(0, 50, g_link_param2, 5);

    /* 电池电压 */
    screen_float(60, 50, s_battery_voltage, 2, 2);
    screen_text(108, 50, "V");

    screen_refresh();
}

/* ------------------------------------------------------------ 数据回传 */

static int battery_percent(float voltage)
{
    int percent;

    if (voltage < BAT_VOLT_EMPTY) {
        return 0;
    }
    if (voltage > BAT_VOLT_FULL) {
        return 100;
    }

    percent = (int)(((voltage - BAT_VOLT_EMPTY) / (BAT_VOLT_FULL - BAT_VOLT_EMPTY)) * 100.0f + 0.5f);

    if (percent > 100) {
        percent = 100;
    }
    if (percent <= 0) {
        percent = 0;
    }

    return percent;
}

static void report_to_app(void)
{
    static uint8_t showtime = 0;

    if (g_link_report_request == 1) {
        g_link_report_request = 0;
        sprintf(s_frame_c, "{C%d:%d}$", g_link_param1, g_link_param2);
        link_send_text(s_frame_c);
        return;
    }

    /* 两个页面交替发送，降低串口占用 */
    showtime++;
    if (showtime == 1) {
        int percent = battery_percent(s_battery_voltage);

        sprintf(s_frame_a, "{A%d:%d:%d:%d}$",
                (int)chassis_actual_rpm(WHEEL_A),
                (int)chassis_actual_rpm(WHEEL_B),
                percent,
                (int)s_imu_attitude.yaw);
        link_send_text(s_frame_a);
    } else if (showtime == 2) {
        sprintf(s_frame_b, "{B%d:%d:%d}$",
                (int)s_imu_attitude.pitch,
                (int)s_imu_attitude.roll,
                (int)s_imu_attitude.yaw);
        link_send_text(s_frame_b);
    } else {
        showtime = 0;
    }
}

/* ------------------------------------------------------------ 主函数 */

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    tick_init(1000);                        /* 1ms 系统节拍 */

    debug_uart_init(DBG_UART_BAUD);
    io_init();                              /* 指示灯 / 蜂鸣器 / 按键 */
    screen_init();                             /* 屏幕 */

    bat_adc_init();                         /* 电池采样 */
    motor_init();                           /* 四路电机 + 舵机 PWM */
    encoder_init();                         /* 四路编码器 */
    aux_motor_init();                       /* 叶片电机：按 AUX_MOTOR_DEFAULT_DUTY 起转，须放在所有定时器之后 */

    i2c_init();
    imu_init();                             /* 九轴传感器 */

    chassis_init();                         /* 档位与 PI 状态归位 */
    link_init(LINK_UART_BAUD);              /* 蓝牙串口，开始接收 */

    control_timer_init();                   /* 最后启动控制节拍 */

    while (1) {
        /* 电池电压：中断只是置标志，实际转换在主循环里做 */
        if (s_adc_request == 1) {
            s_adc_request = 0;
            s_battery_voltage = bat_adc_voltage();
        }

        /* 提示音 */
        if (s_buzzer_ticks != 0) {
            s_buzzer_ticks--;
            buzzer_write(1);
        } else {
            buzzer_write(0);
        }

        display_update();
        report_to_app();

        delay_ms(10);
    }
}
