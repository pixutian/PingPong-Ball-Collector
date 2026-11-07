/*
 * encoder.c
 * -----------------------------------------------------------------------------
 * 编码器计数器配置。
 *
 * 定时器工作在"编码器接口模式"：硬件按 A/B 两相的边沿直接加减计数，
 * 软件只需周期性地读出 CNT 再清零，读到的差值就是本周期的脉冲数。
 */

#include "encoder.h"
#include "board.h"

#define ENCODER_COUNTER_PERIOD   0xFFFFu

/* 单个复用引脚 */
static void encoder_pin(GPIO_TypeDef *port, uint16_t pin, uint8_t src, uint8_t af)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin   = pin;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_OType = GPIO_OType_OD;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(port, &gpio);

    GPIO_PinAFConfig(port, src, af);
}

/* 定时器部分：只配置计数 + 编码器接口，与引脚无关 */
static void encoder_timer(TIM_TypeDef *tim)
{
    TIM_TimeBaseInitTypeDef base;
    TIM_ICInitTypeDef ic;

    base.TIM_Prescaler     = 0;
    base.TIM_Period        = ENCODER_COUNTER_PERIOD;
    base.TIM_ClockDivision = TIM_CKD_DIV1;
    base.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim, &base);

    /* TI12 模式：A/B 两相的上升沿都计数，等效四倍频 */
    TIM_EncoderInterfaceConfig(tim, TIM_EncoderMode_TI12,
                               TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

    TIM_ICStructInit(&ic);
    ic.TIM_ICFilter = 0;
    TIM_ICInit(tim, &ic);

    TIM_SetCounter(tim, 0);
    TIM_Cmd(tim, ENABLE);
}

void encoder_init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 |
                           RCC_APB1Periph_TIM4 | RCC_APB1Periph_TIM5, ENABLE);
    RCC_AHB1PeriphClockCmd(ENC_GPIO_CLK, ENABLE);

    /* A 轮：PA15 + PB3，跨两个端口 */
    encoder_pin(ENC_A_AF_PORT_A, ENC_A_PIN_A, GPIO_PinSource15, GPIO_AF_TIM2);
    encoder_pin(ENC_A_AF_PORT_B, ENC_A_PIN_B, GPIO_PinSource3,  GPIO_AF_TIM2);
    encoder_timer(ENC_A_TIM);

    /* B 轮：PB4 + PB5 */
    encoder_pin(ENC_B_GPIO_PORT, ENC_B_PIN_A, GPIO_PinSource4, GPIO_AF_TIM3);
    encoder_pin(ENC_B_GPIO_PORT, ENC_B_PIN_B, GPIO_PinSource5, GPIO_AF_TIM3);
    encoder_timer(ENC_B_TIM);

    /* C 轮：PB6 + PB7 */
    encoder_pin(ENC_C_GPIO_PORT, ENC_C_PIN_A, GPIO_PinSource6, GPIO_AF_TIM4);
    encoder_pin(ENC_C_GPIO_PORT, ENC_C_PIN_B, GPIO_PinSource7, GPIO_AF_TIM4);
    encoder_timer(ENC_C_TIM);

    /* D 轮：PA0 + PA1 */
    encoder_pin(ENC_D_GPIO_PORT, ENC_D_PIN_A, GPIO_PinSource0, GPIO_AF_TIM5);
    encoder_pin(ENC_D_GPIO_PORT, ENC_D_PIN_B, GPIO_PinSource1, GPIO_AF_TIM5);
    encoder_timer(ENC_D_TIM);
}

int encoder_read(encoder_wheel_t wheel)
{
    int pulses;
    TIM_TypeDef *tim;

    switch (wheel) {
        case ENC_WHEEL_A: tim = ENC_A_TIM; break;
        case ENC_WHEEL_B: tim = ENC_B_TIM; break;
        case ENC_WHEEL_C: tim = ENC_C_TIM; break;
        case ENC_WHEEL_D: tim = ENC_D_TIM; break;
        default:          return 0;
    }

    pulses   = (short)tim->CNT;   /* 16 位计数器转成有符号数，正负即方向 */
    tim->CNT = 0;

    return pulses;
}
