#ifndef C30D_YELLOW_MOTOR_H
#define C30D_YELLOW_MOTOR_H

#include "stm32f4xx.h"

/*
 * 黄色电机驱动 (TB6612 单路 PWM 调速)
 * ---------------------------------------------------------------
 * 硬件接线 (与原 zhou 工程一致):
 *   PC6  -> TB6612 PWMA   (TIM8_CH1, AF3)
 *   AO1/AO2 -> 电机两端
 *   VCC, STBY, AIN1 -> 3.3 V
 *   AIN2 -> GND           (硬件固定正转方向, 反转需改接线或GPIO控制方向脚)
 *   VM   -> 电机独立电源 (与电机电压匹配, 切勿用底盘12V直接带低压TT电机)
 *   所有电源共地 GND
 *
 * 占用资源: TIM8 整体 (含其它通道), PC6.
 *   在本工程中 TIM8 与 PC6 均未被其它外设使用, 可安全占用.
 *
 * 用法:
 *   1. main.c 顶部 #include "yellow_motor.h"
 *   2. 在所有外设初始化完成后, while(1) 之前调用 YellowMotor_Init();
 *   3. 运行时调用 YellowMotor_SetSpeed(0..100) 修改占空比;
 *      本工程已绑定蓝牙 APP 按键:
 *        'T' (dirkey=20) -> 黄色电机占空比 +10%
 *        'U' (dirkey=21) -> 黄色电机占空比 -10%
 *      占空比范围 0..100, 默认 50%.
 */

/* 上电后黄色电机默认占空比 0..100 */
#ifndef YELLOW_MOTOR_DEFAULT_SPEED
#define YELLOW_MOTOR_DEFAULT_SPEED 80
#endif

/* 运行时占空比 0..100 (百分比, 非RPM) */
volatile uint8_t yellow_motor_speed = YELLOW_MOTOR_DEFAULT_SPEED;

static void YellowMotor_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;

    RCC->APB2RSTR |= RCC_APB2RSTR_TIM8RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM8RST;

    /* PC6: AF3 = TIM8_CH1, push-pull output */
    GPIOC->MODER   = (GPIOC->MODER   & ~(3UL << 12)) | (2UL << 12);
    GPIOC->OTYPER &= ~(1UL << 6);
    GPIOC->OSPEEDR = (GPIOC->OSPEEDR & ~(3UL << 12)) | (1UL << 12);
    GPIOC->PUPDR  &= ~(3UL << 12);
    GPIOC->AFR[0]  = (GPIOC->AFR[0] & ~(15UL << 24)) | (3UL << 24);

    /* 10000 counts/cycle: 16.8 kHz when TIM8 clock = 168 MHz */
    TIM8->PSC  = 0;
    TIM8->ARR  = 9999;
    TIM8->CCR1 = (uint32_t)yellow_motor_speed * 100UL;
    TIM8->CCMR1 = (6UL << 4) | TIM_CCMR1_OC1PE;   /* PWM mode 1 */
    TIM8->CCER  = TIM_CCER_CC1E;
    TIM8->CR1   = TIM_CR1_ARPE;
    TIM8->EGR   = TIM_EGR_UG;
    TIM8->SR    = 0;
    TIM8->BDTR  = TIM_BDTR_MOE;
    TIM8->CR1  |= TIM_CR1_CEN;
}

/* 在线修改黄色电机占空比 percent: 0..100, 自动限幅 */
static inline void YellowMotor_SetSpeed(uint8_t percent)
{
    if (percent > 100U) percent = 100U;
    yellow_motor_speed = percent;
    TIM8->CCR1 = (uint32_t)percent * 100UL;
}

#endif /* C30D_YELLOW_MOTOR_H */
