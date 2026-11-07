/*
 * serial.c
 * -----------------------------------------------------------------------------
 * 两个串口的初始化与阻塞发送。
 *
 * 分工：USART1 只发（调试打印），USART2 收发并开启接收中断，
 * 中断服务函数在 app_link.c 里，和协议解析放在一起。
 */

#include "serial.h"
#include "board.h"
#include <stdio.h>

static void uart_gpio_init(GPIO_TypeDef *port, uint16_t tx_pin, uint16_t rx_pin,
                           uint8_t tx_src, uint8_t rx_src, uint8_t af)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin   = tx_pin | rx_pin;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(port, &gpio);

    GPIO_PinAFConfig(port, tx_src, af);
    GPIO_PinAFConfig(port, rx_src, af);
}

static void uart_parameter_init(USART_TypeDef *uart, uint32_t baud)
{
    USART_InitTypeDef usart;

    usart.USART_BaudRate            = baud;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(uart, &usart);
}

void debug_uart_init(uint32_t baud)
{
    RCC_AHB1PeriphClockCmd(DBG_UART_GPIO_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(DBG_UART_CLK, ENABLE);

    uart_gpio_init(DBG_UART_GPIO_PORT, DBG_UART_TX_PIN, DBG_UART_RX_PIN,
                   GPIO_PinSource9, GPIO_PinSource10, GPIO_AF_USART1);
    uart_parameter_init(DBG_UART, baud);

    USART_Cmd(DBG_UART, ENABLE);
}

void link_uart_init(uint32_t baud)
{
    NVIC_InitTypeDef nvic;

    RCC_AHB1PeriphClockCmd(LINK_UART_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(LINK_UART_CLK, ENABLE);

    uart_gpio_init(LINK_UART_GPIO_PORT, LINK_UART_TX_PIN, LINK_UART_RX_PIN,
                   GPIO_PinSource5, GPIO_PinSource6, GPIO_AF_USART2);
    uart_parameter_init(LINK_UART, baud);

    nvic.NVIC_IRQChannel                   = USART2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 5;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    USART_ITConfig(LINK_UART, USART_IT_RXNE, ENABLE);
    USART_Cmd(LINK_UART, ENABLE);
}

void link_uart_send(const char *text)
{
    while (*text != '\0') {
        while ((LINK_UART->SR & USART_FLAG_TXE) == 0) {
            /* 等待发送寄存器空 */
        }
        LINK_UART->DR = (uint8_t)(*text++);
    }
}

/* printf 重定向到调试串口，便于在线观察 */
#ifdef __CC_ARM
#pragma import(__use_no_semihosting)

struct __FILE {
    int handle;
};

FILE __stdout;

void _sys_exit(int x)
{
    x = x;
}

int fputc(int ch, FILE *f)
{
    (void)f;
    while ((DBG_UART->SR & USART_FLAG_TXE) == 0) {
        /* 等待发送寄存器空 */
    }
    DBG_UART->DR = (uint8_t)ch;

    return ch;
}
#endif
