/*
 * board.h
 * -----------------------------------------------------------------------------
 * 板级资源映射表：把每个功能占用的 GPIO / 定时器通道集中写在这里，
 * 换板子时只改这张表，驱动代码不用动。
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f4xx.h"

/* ---------------------------- 状态指示灯 ------------------------------ */
#define LED_CLK          RCC_AHB1Periph_GPIOE
#define LED_PORT         GPIOE
#define LED_PIN          GPIO_Pin_8

/* ------------------------------ 蜂鸣器 -------------------------------- */
#define BEEP_CLK         RCC_AHB1Periph_GPIOA
#define BEEP_PORT        GPIOA
#define BEEP_PIN         GPIO_Pin_8

/* --------------------------- 按键 / 急停 ------------------------------ */
#define KEY_CLK          RCC_AHB1Periph_GPIOE
#define KEY_PORT         GPIOE
#define KEY_PIN          GPIO_Pin_0

#define ESTOP_CLK        RCC_AHB1Periph_GPIOD
#define ESTOP_PORT       GPIOD
#define ESTOP_PIN        GPIO_Pin_3

/* ------------------------------ 电池采样 ------------------------------ */
#define BAT_ADC          ADC1
#define BAT_GPIO_CLK     RCC_AHB1Periph_GPIOB
#define BAT_GPIO_PORT    GPIOB
#define BAT_GPIO_PIN     GPIO_Pin_0        /* ADC1_IN8 */

/* ---------------------------- 电机 PWM 输出 --------------------------- */
/*
 * 四路直流电机，每路占两个比较通道：一个是正向通道，一个是反向通道。
 * 通道分配（与硬件驱动板一致，改之前先看原理图）：
 *   A 轮：TIM10_CH1 / TIM11_CH1
 *   B 轮：TIM9_CH1  / TIM9_CH2
 *   C 轮：TIM1_CH2  / TIM1_CH1
 *   D 轮：TIM1_CH4  / TIM1_CH3
 * 舵机：TIM12_CH1 / TIM12_CH2（PB14 / PB15）
 */
#define MOTOR_A_GPIO_CLK     RCC_AHB1Periph_GPIOB
#define MOTOR_A_GPIO_PORT    GPIOB
#define MOTOR_A_PIN          GPIO_Pin_8     /* PB8  -> TIM10_CH1 */

#define MOTOR_B_GPIO_CLK     RCC_AHB1Periph_GPIOE
#define MOTOR_B_GPIO_PORT    GPIOE
#define MOTOR_B_PIN          (GPIO_Pin_5 | GPIO_Pin_6)   /* PE5/PE6 -> TIM9_CH1/CH2 */

#define MOTOR_CD_GPIO_CLK    RCC_AHB1Periph_GPIOE
#define MOTOR_CD_GPIO_PORT   GPIOE
#define MOTOR_CD_PIN         (GPIO_Pin_9 | GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14)

#define MOTOR_D_GPIO_CLK     RCC_AHB1Periph_GPIOB
#define MOTOR_D_GPIO_PORT    GPIOB
#define MOTOR_D_PIN          GPIO_Pin_9     /* PB9  -> TIM11_CH1 */

#define SERVO_GPIO_CLK       RCC_AHB1Periph_GPIOB
#define SERVO_GPIO_PORT      GPIOB
#define SERVO_GPIO_PIN       (GPIO_Pin_14 | GPIO_Pin_15)  /* TIM12_CH1/CH2 */

/* ---------------------------- 编码器输入 ----------------------------- */
/*
 * 四个定时器的编码器接口模式：
 *   TIM2 -> PA15 / PB3    编码器 A
 *   TIM3 -> PB4  / PB5    编码器 B
 *   TIM4 -> PB6  / PB7    编码器 C
 *   TIM5 -> PA0  / PA1    编码器 D
 * 全部使用 TI12 模式、上升沿计数（即 AB 两相四倍频）。
 */
#define ENC_GPIO_CLK         (RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB)

#define ENC_A_TIM            TIM2
#define ENC_A_GPIO_PORT      GPIOA
#define ENC_A_GPIO_CLK       RCC_AHB1Periph_GPIOA
#define ENC_A_PIN_A          GPIO_Pin_15
#define ENC_A_PIN_B          GPIO_Pin_3
#define ENC_A_AF_PORT_A      GPIOA
#define ENC_A_AF_PORT_B      GPIOB

#define ENC_B_TIM            TIM3
#define ENC_B_GPIO_PORT      GPIOB
#define ENC_B_PIN_A          GPIO_Pin_4
#define ENC_B_PIN_B          GPIO_Pin_5

#define ENC_C_TIM            TIM4
#define ENC_C_GPIO_PORT      GPIOB
#define ENC_C_PIN_A          GPIO_Pin_6
#define ENC_C_PIN_B          GPIO_Pin_7

#define ENC_D_TIM            TIM5
#define ENC_D_GPIO_PORT      GPIOA
#define ENC_D_PIN_A          GPIO_Pin_0
#define ENC_D_PIN_B          GPIO_Pin_1

/* ------------------------------ 串口 -------------------------------- */
#define DBG_UART             USART1
#define DBG_UART_CLK         RCC_APB2Periph_USART1
#define DBG_UART_GPIO_CLK    RCC_AHB1Periph_GPIOA
#define DBG_UART_GPIO_PORT   GPIOA
#define DBG_UART_TX_PIN      GPIO_Pin_9
#define DBG_UART_RX_PIN      GPIO_Pin_10

#define LINK_UART            USART2
#define LINK_UART_CLK        RCC_APB1Periph_USART2
#define LINK_UART_GPIO_CLK   RCC_AHB1Periph_GPIOD
#define LINK_UART_GPIO_PORT  GPIOD
#define LINK_UART_TX_PIN     GPIO_Pin_5
#define LINK_UART_RX_PIN     GPIO_Pin_6

/* --------------------------- OLED (SSD1306) -------------------------- */
#define SCREEN_GPIO_CLK         RCC_AHB1Periph_GPIOD
#define SCREEN_GPIO_PORT        GPIOD
#define SCREEN_RST_PIN          GPIO_Pin_12
#define SCREEN_DC_PIN           GPIO_Pin_11
#define SCREEN_SCL_PIN          GPIO_Pin_14
#define SCREEN_SDA_PIN          GPIO_Pin_13

#define SCREEN_RST_HIGH()       GPIO_SetBits(SCREEN_GPIO_PORT, SCREEN_RST_PIN)
#define SCREEN_RST_LOW()        GPIO_ResetBits(SCREEN_GPIO_PORT, SCREEN_RST_PIN)
#define SCREEN_DC_HIGH()        GPIO_SetBits(SCREEN_GPIO_PORT, SCREEN_DC_PIN)
#define SCREEN_DC_LOW()         GPIO_ResetBits(SCREEN_GPIO_PORT, SCREEN_DC_PIN)
#define SCREEN_SCL_HIGH()       GPIO_SetBits(SCREEN_GPIO_PORT, SCREEN_SCL_PIN)
#define SCREEN_SCL_LOW()        GPIO_ResetBits(SCREEN_GPIO_PORT, SCREEN_SCL_PIN)
#define SCREEN_SDA_HIGH()       GPIO_SetBits(SCREEN_GPIO_PORT, SCREEN_SDA_PIN)
#define SCREEN_SDA_LOW()        GPIO_ResetBits(SCREEN_GPIO_PORT, SCREEN_SDA_PIN)

/* ------------------------------ 软件 IIC ----------------------------- */
#define IIC_GPIO_CLK         RCC_AHB1Periph_GPIOB
#define IIC_GPIO_PORT        GPIOB
#define IIC_SCL_PIN          GPIO_Pin_10
#define IIC_SDA_PIN          GPIO_Pin_11

/* ---------------------------- 叶片电机 (TB6612) ------------------------
 * 单路 PWM 调速，独立于底盘：PC6 -> TIM8_CH1，方向由 TB6612 的 AIN 脚硬件固定。
 * 转速 = 占空比，换算在 motor.c 的 aux_motor_set_duty() 里，默认值在
 * app_config.h 的 AUX_MOTOR_DEFAULT_DUTY。 */
#define AUX_GPIO_CLK         RCC_AHB1Periph_GPIOC
#define AUX_GPIO_PORT        GPIOC
#define AUX_PWM_PIN          GPIO_Pin_6     /* PC6 -> TIM8_CH1，AF3 */
#define AUX_TIM              TIM8
#define AUX_TIM_CLK          RCC_APB2Periph_TIM8
#define AUX_PWM_ARR          9999          /* 约 16.8kHz PWM；占空比 100% 对应比较值 10000 */

#endif /* BOARD_H */
