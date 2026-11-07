/*
 * stm32f4xx_it.c
 * -----------------------------------------------------------------------------
 * Cortex-M4 内核异常的统一处理。
 *
 * 说明：外设中断不写在这里，各自放在所属模块里，职责更清晰：
 *   SysTick        -> Hardware/clock.c
 *   TIM7 控制节拍  -> App/app_main.c
 *   USART2 蓝牙    -> App/app_link.c
 */

#include "stm32f4xx.h"

/* 不可屏蔽中断 */
void NMI_Handler(void)
{
}

/* 硬件错误：停在这里，方便调试器接管 */
void HardFault_Handler(void)
{
    while (1) {
    }
}

/* 存储管理错误 */
void MemManage_Handler(void)
{
    while (1) {
    }
}

/* 总线错误 */
void BusFault_Handler(void)
{
    while (1) {
    }
}

/* 用法错误 */
void UsageFault_Handler(void)
{
    while (1) {
    }
}

/* 系统服务调用 */
void SVC_Handler(void)
{
}

/* 调试监视 */
void DebugMon_Handler(void)
{
}

/* 可挂起系统服务 */
void PendSV_Handler(void)
{
}
