/*
 * motor.c
 * -----------------------------------------------------------------------------
 * 电机与舵机 PWM 输出。
 *
 * 硬件分配：
 *   TIM1  CH1~CH4 -> PE9 / PE11 / PE13 / PE14   C 轮与 D 轮
 *   TIM9  CH1~CH2 -> PE5 / PE6                  B 轮
 *   TIM10 CH1     -> PB8                        A 轮正向
 *   TIM11 CH1     -> PB9                        A 轮反向
 *   TIM12 CH1~CH2 -> PB14 / PB15                舵机
 *
 * 说明：H 桥每个桥臂由两路 PWM 控制，一路给满、一路给 |duty|，
 * 两者之差就是加在电机上的有效电压，符号决定方向。
 */

#include "motor.h"
#include "board.h"
#include "app_config.h"

/* ======================= 叶片（分机）电机转速 ==========================
 * 这里就是控制叶片电机转速的地方，三处配套看：
 *   1) s_aux_duty      ：当前占空比（%），上电初值取 app_config.h 的
 *                        AUX_MOTOR_DEFAULT_DUTY，想改默认转速改那个宏；
 *   2) aux_motor_init()：上电把 s_aux_duty 换算成比较值写进 TIM8->CCR1，
 *                        同时把 PC6 复用成 TIM8_CH1；
 *   3) aux_motor_set_duty()：运行中改转速，APP 的 T/U 键（±10%）走这里。
 *
 * 换算关系：CCR1 = 占空比 * (AUX_PWM_ARR + 1) / 100，
 * 即 100% -> 10000、50% -> 5000（AUX_PWM_ARR 见 board.h，为 9999）。
 * ======================================================================= */
static uint8_t s_aux_duty = AUX_MOTOR_DEFAULT_DUTY;   /* 叶片电机占空比，单位 % */

/* ---------------------------------------------------------------- 底层配置 */

static void motor_gpio_init(GPIO_TypeDef *port, uint16_t pins, uint16_t af_source,
                            uint8_t af_index, uint16_t pins_a, uint16_t pins_b)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin   = pins;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(port, &gpio);

    if (pins_a) {
        GPIO_PinAFConfig(port, pins_a, af_index);
    }
    if (pins_b) {
        GPIO_PinAFConfig(port, pins_b, af_index);
    }
    (void)af_source;
}

/* 通用 PWM 定时器初始化：只有基础计数 + 指定数量的比较通道 */
static void pwm_timer_init(TIM_TypeDef *tim, uint16_t period, uint16_t prescaler,
                           uint8_t channel_mask, uint8_t output_enable)
{
    TIM_TimeBaseInitTypeDef base;
    TIM_OCInitTypeDef       oc;

    base.TIM_Period        = period;
    base.TIM_Prescaler     = prescaler;
    base.TIM_ClockDivision = TIM_CKD_DIV1;
    base.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim, &base);

    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    oc.TIM_Pulse       = 0;

    if (channel_mask & 0x01) { TIM_OC1Init(tim, &oc); TIM_OC1PreloadConfig(tim, TIM_OCPreload_Enable); }
    if (channel_mask & 0x02) { TIM_OC2Init(tim, &oc); TIM_OC2PreloadConfig(tim, TIM_OCPreload_Enable); }
    if (channel_mask & 0x04) { TIM_OC3Init(tim, &oc); TIM_OC3PreloadConfig(tim, TIM_OCPreload_Enable); }
    if (channel_mask & 0x08) { TIM_OC4Init(tim, &oc); TIM_OC4PreloadConfig(tim, TIM_OCPreload_Enable); }

    if (output_enable) {
        TIM_CtrlPWMOutputs(tim, ENABLE);   /* 高级定时器必须开主输出 */
    }

    TIM_ARRPreloadConfig(tim, ENABLE);
    TIM_Cmd(tim, ENABLE);
}

/* ------------------------------------------------------------ 对外接口实现 */

void motor_init(void)
{
    /* --- A 轮：TIM10 / TIM11 --- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10 | RCC_APB2Periph_TIM11, ENABLE);
    RCC_AHB1PeriphClockCmd(MOTOR_A_GPIO_CLK | MOTOR_D_GPIO_CLK, ENABLE);

    motor_gpio_init(MOTOR_A_GPIO_PORT, MOTOR_A_PIN, 0,
                    GPIO_AF_TIM10, GPIO_PinSource8, 0);
    motor_gpio_init(MOTOR_D_GPIO_PORT, MOTOR_D_PIN, 0,
                    GPIO_AF_TIM11, GPIO_PinSource9, 0);

    pwm_timer_init(TIM10, MOTOR_PWM_ARR, MOTOR_PWM_PSC, 0x01, 0);
    pwm_timer_init(TIM11, MOTOR_PWM_ARR, MOTOR_PWM_PSC, 0x01, 1);

    /* --- B 轮：TIM9 CH1/CH2 --- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE);
    RCC_AHB1PeriphClockCmd(MOTOR_B_GPIO_CLK, ENABLE);

    motor_gpio_init(MOTOR_B_GPIO_PORT, MOTOR_B_PIN, 0,
                    GPIO_AF_TIM9, GPIO_PinSource5, GPIO_PinSource6);

    pwm_timer_init(TIM9, MOTOR_PWM_ARR, MOTOR_PWM_PSC, 0x03, 0);

    /* --- C / D 轮：TIM1 CH1~CH4 --- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_AHB1PeriphClockCmd(MOTOR_CD_GPIO_CLK, ENABLE);

    {
        GPIO_InitTypeDef gpio;

        gpio.GPIO_Pin   = MOTOR_CD_PIN;
        gpio.GPIO_Mode  = GPIO_Mode_AF;
        gpio.GPIO_Speed = GPIO_Speed_100MHz;
        gpio.GPIO_OType = GPIO_OType_PP;
        gpio.GPIO_PuPd  = GPIO_PuPd_UP;
        GPIO_Init(MOTOR_CD_GPIO_PORT, &gpio);

        GPIO_PinAFConfig(MOTOR_CD_GPIO_PORT, GPIO_PinSource9,  GPIO_AF_TIM1);
        GPIO_PinAFConfig(MOTOR_CD_GPIO_PORT, GPIO_PinSource11, GPIO_AF_TIM1);
        GPIO_PinAFConfig(MOTOR_CD_GPIO_PORT, GPIO_PinSource13, GPIO_AF_TIM1);
        GPIO_PinAFConfig(MOTOR_CD_GPIO_PORT, GPIO_PinSource14, GPIO_AF_TIM1);
    }

    pwm_timer_init(TIM1, MOTOR_PWM_ARR, MOTOR_PWM_PSC, 0x0F, 1);

    /* --- 舵机：TIM12 CH1/CH2，50Hz --- */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM12, ENABLE);
    RCC_AHB1PeriphClockCmd(SERVO_GPIO_CLK, ENABLE);

    motor_gpio_init(SERVO_GPIO_PORT, SERVO_GPIO_PIN, 0,
                    GPIO_AF_TIM12, GPIO_PinSource14, GPIO_PinSource15);

    pwm_timer_init(TIM12, SERVO_PWM_ARR, SERVO_PWM_PSC, 0x03, 1);
    TIM12->CCR1 = 1500;
    TIM12->CCR2 = 1500;
}

/* 单路电机输出：pos 与 neg 是两个桥臂的比较寄存器 */
static void motor_leg(volatile uint32_t *pos, volatile uint32_t *neg, int duty)
{
    if (duty < 0) {
        *pos = MOTOR_PWM_ARR;
        *neg = MOTOR_PWM_ARR + duty;
    } else {
        *neg = MOTOR_PWM_ARR;
        *pos = MOTOR_PWM_ARR - duty;
    }
}

void motor_set_duty(int duty_a, int duty_b, int duty_c, int duty_d)
{
    motor_leg(&TIM10->CCR1, &TIM11->CCR1, duty_a);
    motor_leg(&TIM9->CCR1,  &TIM9->CCR2,  duty_b);
    motor_leg(&TIM1->CCR2,  &TIM1->CCR1,  duty_c);
    motor_leg(&TIM1->CCR4,  &TIM1->CCR3,  duty_d);
}

void servo_set_angle(float degree)
{
    int compare;

    if (degree < 0.0f) {
        degree = 0.0f;
    }
    if (degree > SERVO_TRAVEL_DEG) {
        degree = SERVO_TRAVEL_DEG;
    }

    compare = (int)(SERVO_ANGLE_REF_VOLT +
                    ((float)SERVO_ANGLE_SPAN * degree / SERVO_TRAVEL_DEG));

    if (compare < 500)  { compare = 500; }
    if (compare > 2500) { compare = 2500; }

    TIM12->CCR2 = (uint32_t)compare;
}

/* -------------------------------------------------------------- 叶片电机 */

/* 叶片（分机）电机初始化：PC6 -> TIM8_CH1，并用默认占空比立刻起转 */
void aux_motor_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;

    RCC->APB2RSTR |= RCC_APB2RSTR_TIM8RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM8RST;

    /* PC6 复用为 TIM8_CH1 */
    GPIOC->MODER   = (GPIOC->MODER   & ~(3UL << 12)) | (2UL << 12);
    GPIOC->OTYPER &= ~(1UL << 6);
    GPIOC->OSPEEDR = (GPIOC->OSPEEDR & ~(3UL << 12)) | (1UL << 12);
    GPIOC->PUPDR  &= ~(3UL << 12);
    GPIOC->AFR[0]  = (GPIOC->AFR[0] & ~(15UL << 24)) | (3UL << 24);

    TIM8->PSC   = 0;
    TIM8->ARR   = AUX_PWM_ARR;
    TIM8->CCR1  = (uint32_t)s_aux_duty * ((AUX_PWM_ARR + 1) / 100);
    TIM8->CCMR1 = (6UL << 4) | TIM_CCMR1_OC1PE;   /* PWM 模式 1 + 预装载 */
    TIM8->CCER  = TIM_CCER_CC1E;
    TIM8->CR1   = TIM_CR1_ARPE;
    TIM8->EGR   = TIM_EGR_UG;
    TIM8->SR    = 0;
    TIM8->BDTR  = TIM_BDTR_MOE;
    TIM8->CR1  |= TIM_CR1_CEN;
}

/* 改叶片电机转速：percent 为占空比百分比 0~100，超范围自动限幅。
 * 调用后立即生效，返回值只在下次读取时反映。 */
void aux_motor_set_duty(uint8_t percent)
{
    if (percent > 100U) {
        percent = 100U;
    }

    s_aux_duty  = percent;
    TIM8->CCR1  = (uint32_t)percent * ((AUX_PWM_ARR + 1) / 100);
}

uint8_t aux_motor_get_duty(void)
{
    return s_aux_duty;
}
