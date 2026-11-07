/*
 * i2c_soft.c
 * -----------------------------------------------------------------------------
 * GPIO 模拟 IIC 主机。
 *
 * 时序：起始 -> 器件地址 -> (寄存器地址) -> 数据 -> 停止。
 * SDA 需要在读的时候切成输入，所以每个阶段都重新配置一次引脚方向。
 * IIC_DELAY_LOOPS 用来把总线速度压到器件能接受的范围，主频越高值越大。
 */

#include "i2c_soft.h"
#include "board.h"

#define IIC_DELAY_LOOPS   25

#define SCL_HIGH()   GPIO_SetBits(IIC_GPIO_PORT, IIC_SCL_PIN)
#define SCL_LOW()    GPIO_ResetBits(IIC_GPIO_PORT, IIC_SCL_PIN)
#define SDA_HIGH()   GPIO_SetBits(IIC_GPIO_PORT, IIC_SDA_PIN)
#define SDA_LOW()    GPIO_ResetBits(IIC_GPIO_PORT, IIC_SDA_PIN)
#define SDA_LEVEL()  GPIO_ReadInputDataBit(IIC_GPIO_PORT, IIC_SDA_PIN)

static uint8_t s_inited = 0;

static void iic_delay(void)
{
    uint8_t n = IIC_DELAY_LOOPS;

    while (n--) {
        /* 空转，靠循环开销延时 */
    }
}

static void sda_as_output(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin   = IIC_SDA_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(IIC_GPIO_PORT, &gpio);
}

static void sda_as_input(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin   = IIC_SDA_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IN;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(IIC_GPIO_PORT, &gpio);
}

void i2c_init(void)
{
    GPIO_InitTypeDef gpio;

    if (s_inited) {
        return;
    }

    RCC_AHB1PeriphClockCmd(IIC_GPIO_CLK, ENABLE);

    gpio.GPIO_Pin   = IIC_SCL_PIN | IIC_SDA_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(IIC_GPIO_PORT, &gpio);

    SCL_HIGH();
    SDA_HIGH();

    s_inited = 1;
}

static void iic_start(void)
{
    sda_as_output();
    SDA_HIGH();
    SCL_HIGH();
    iic_delay();
    SDA_LOW();        /* SCL 高电平期间拉低 SDA = 起始 */
    iic_delay();
    SCL_LOW();
}

static void iic_stop(void)
{
    sda_as_output();
    SCL_LOW();
    SDA_LOW();
    iic_delay();
    SCL_HIGH();
    iic_delay();
    SDA_HIGH();       /* SCL 高电平期间拉高 SDA = 停止 */
}

/* 返回 1 表示从机应答，0 表示超时无应答 */
static uint8_t iic_wait_ack(uint32_t timeout)
{
    uint32_t n = 0;

    sda_as_input();
    SDA_HIGH();
    iic_delay();
    SCL_HIGH();
    iic_delay();

    while (SDA_LEVEL()) {
        if (++n > timeout) {
            iic_stop();
            return 0;
        }
    }

    SCL_LOW();
    return 1;
}

static void iic_send_byte(uint8_t byte)
{
    uint8_t i;

    sda_as_output();
    SCL_LOW();

    for (i = 0; i < 8; i++) {
        if (byte & 0x80) {
            SDA_HIGH();
        } else {
            SDA_LOW();
        }
        byte <<= 1;
        iic_delay();
        SCL_HIGH();
        iic_delay();
        SCL_LOW();
    }
}

static uint8_t iic_read_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t byte = 0;

    sda_as_input();

    for (i = 0; i < 8; i++) {
        SCL_LOW();
        iic_delay();
        SCL_HIGH();
        iic_delay();

        byte <<= 1;
        if (SDA_LEVEL()) {
            byte |= 0x01;
        }
    }

    if (ack) {
        /* 主机应答：拉低 SDA 一个时钟 */
        SCL_LOW();
        sda_as_output();
        SDA_LOW();
        iic_delay();
        SCL_HIGH();
        iic_delay();
        SCL_LOW();
    } else {
        /* 主机不应答：保持 SDA 高 */
        SCL_LOW();
        sda_as_output();
        SDA_HIGH();
        iic_delay();
        SCL_HIGH();
        iic_delay();
        SCL_LOW();
    }

    return byte;
}

uint8_t i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *buf, uint16_t len, uint32_t timeout)
{
    uint16_t i;

    iic_start();

    iic_send_byte((uint8_t)(addr7 << 1));
    if (!iic_wait_ack(timeout)) {
        iic_stop();
        return I2C_ERR_NACK;
    }

    iic_send_byte(reg);
    if (!iic_wait_ack(timeout)) {
        iic_stop();
        return I2C_ERR_NACK;
    }

    for (i = 0; i < len; i++) {
        iic_send_byte(buf[i]);
        if (!iic_wait_ack(timeout)) {
            iic_stop();
            return I2C_ERR_NACK;
        }
    }

    iic_stop();

    return I2C_OK;
}

uint8_t i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len, uint32_t timeout)
{
    uint16_t i;

    iic_start();

    iic_send_byte((uint8_t)(addr7 << 1));
    if (!iic_wait_ack(timeout)) {
        iic_stop();
        return I2C_ERR_NACK;
    }

    iic_send_byte(reg);
    if (!iic_wait_ack(timeout)) {
        iic_stop();
        return I2C_ERR_NACK;
    }

    iic_start();                                  /* 重启，切到读方向 */
    iic_send_byte((uint8_t)((addr7 << 1) | 0x01));
    if (!iic_wait_ack(timeout)) {
        iic_stop();
        return I2C_ERR_NACK;
    }

    for (i = 0; i < len; i++) {
        buf[i] = iic_read_byte((i == (len - 1)) ? 0 : 1);
    }

    iic_stop();

    return I2C_OK;
}
