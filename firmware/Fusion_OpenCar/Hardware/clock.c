/*
 * clock.c
 * -----------------------------------------------------------------------------
 * SysTick 时基实现。
 *
 * 时钟源取 HCLK/8（21MHz @168MHz 主频），重装值 = 时钟 / 目标频率。
 * 中断里只做一次计数自增，因此节拍精度由 SysTick 硬件保证，不受主循环影响。
 */

#include "clock.h"

static volatile uint32_t s_ms_count = 0;   /* 毫秒计数 */
static uint32_t s_tick_freq  = 0;          /* 节拍频率 */
static uint32_t s_reload     = 0;          /* 重装值，delay_us 需要 */

void tick_init(uint32_t freq_hz)
{
    s_tick_freq = freq_hz;
    s_reload    = (SystemCoreClock / 8u) / freq_hz;

    NVIC_SetPriority(SysTick_IRQn, 0);          /* 时基优先级最高 */

    /* 位 2 为时钟源（0 = HCLK/8），位 1 中断使能，位 0 计数器使能 */
    SysTick->CTRL &= ~(1uL << 2);
    SysTick->LOAD  = s_reload - 1u;
    SysTick->VAL   = 0;
    SysTick->CTRL |= (1uL << 1) | (1uL << 0);
}

void SysTick_Handler(void)
{
    s_ms_count++;
}

uint32_t tick_ms(void)
{
    return s_ms_count;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = s_ms_count;
    uint32_t wait  = ms * (s_tick_freq / 1000u);

    while ((s_ms_count - start) < wait) {
        /* 等待节拍 */
    }
}

void delay_us(uint32_t us)
{
    uint32_t cycles_per_us = (SystemCoreClock / 8u) / 1000000u;
    uint32_t target = us * cycles_per_us;
    uint32_t start  = SysTick->VAL;
    uint32_t elapsed;

    do {
        uint32_t now = SysTick->VAL;
        /* 计数器向下计数，回绕时按一个重装周期拼接 */
        if (now <= start) {
            elapsed = start - now;
        } else {
            elapsed = s_reload - now + start;
        }
    } while (elapsed < target);
}
