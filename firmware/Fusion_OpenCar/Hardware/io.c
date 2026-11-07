/*
 * io.c
 * -----------------------------------------------------------------------------
 * 状态灯、蜂鸣器、按键的初始化与读写。
 */

#include "io.h"
#include "board.h"

static void output_pin_init(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin   = pin;
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(port, &gpio);
}

static void input_pin_init(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin   = pin;
    gpio.GPIO_Mode  = GPIO_Mode_IN;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(port, &gpio);
}

void io_init(void)
{
    RCC_AHB1PeriphClockCmd(LED_CLK | BEEP_CLK | KEY_CLK | ESTOP_CLK, ENABLE);

    output_pin_init(LED_PORT,  LED_PIN);
    output_pin_init(BEEP_PORT, BEEP_PIN);
    input_pin_init(KEY_PORT,   KEY_PIN);
    input_pin_init(ESTOP_PORT, ESTOP_PIN);

    led_write(0);
    buzzer_write(0);
}

void led_write(uint8_t on)
{
    if (on) {
        GPIO_SetBits(LED_PORT, LED_PIN);
    } else {
        GPIO_ResetBits(LED_PORT, LED_PIN);
    }
}

void led_toggle(void)
{
    if (GPIO_ReadOutputDataBit(LED_PORT, LED_PIN)) {
        GPIO_ResetBits(LED_PORT, LED_PIN);
    } else {
        GPIO_SetBits(LED_PORT, LED_PIN);
    }
}

void buzzer_write(uint8_t on)
{
    if (on) {
        GPIO_SetBits(BEEP_PORT, BEEP_PIN);
    } else {
        GPIO_ResetBits(BEEP_PORT, BEEP_PIN);
    }
}

uint8_t key_user_pressed(void)
{
    return GPIO_ReadInputDataBit(KEY_PORT, KEY_PIN) ? 0 : 1;
}

uint8_t key_estop_pressed(void)
{
    return GPIO_ReadInputDataBit(ESTOP_PORT, ESTOP_PIN) ? 0 : 1;
}
