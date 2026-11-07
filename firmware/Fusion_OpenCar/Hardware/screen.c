/*
 * screen.c
 * -----------------------------------------------------------------------------
 * SSD1306 128x64 单色屏驱动（GPIO 模拟四线 SPI）。
 *
 * 数据流：绘图函数只改内存里的 GRAM，screen_refresh 一次性把 8 页推给屏幕，
 * 这样主循环刷新时不会出现撕裂，也不占用太多总线时间。
 */

#include "screen.h"
#include "board.h"
#include "clock.h"
#include "font.h"

#define SCREEN_WIDTH        128
#define SCREEN_PAGES        8
#define SCREEN_CMD          0
#define SCREEN_DATA         1

#define CHAR_WIDTH       6      /* 6x12 点阵宽度 */
#define CHAR_PITCH       8      /* 含 2 像素字距 */
#define TEXT_MAX_X       122
#define TEXT_MAX_Y       58

static uint8_t s_gram[SCREEN_WIDTH][SCREEN_PAGES];

/* ------------------------------------------------------------------ 底层 */

static void write_byte(uint8_t dat, uint8_t is_data)
{
    uint8_t i;

    if (is_data) {
        SCREEN_DC_HIGH();
    } else {
        SCREEN_DC_LOW();
    }

    for (i = 0; i < 8; i++) {
        SCREEN_SCL_LOW();
        if (dat & 0x80) {
            SCREEN_SDA_HIGH();
        } else {
            SCREEN_SDA_LOW();
        }
        SCREEN_SCL_HIGH();
        dat <<= 1;
    }

    SCREEN_DC_HIGH();
}

static void set_pixel(uint8_t x, uint8_t y, uint8_t on)
{
    uint8_t page;
    uint8_t bit;

    if (x >= SCREEN_WIDTH || y >= 64) {
        return;
    }

    page = (uint8_t)(7 - y / 8);     /* 屏幕纵向是反的，第 0 页在最下面 */
    bit  = (uint8_t)(y % 8);

    if (on) {
        s_gram[x][page] |= (uint8_t)(1u << (7 - bit));
    } else {
        s_gram[x][page] &= (uint8_t)~(1u << (7 - bit));
    }
}

/* ------------------------------------------------------------------ 刷新 */

void screen_refresh(void)
{
    uint8_t page;
    uint8_t col;

    for (page = 0; page < SCREEN_PAGES; page++) {
        write_byte((uint8_t)(0xB0 + page), SCREEN_CMD);   /* 页地址 */
        write_byte(0x00, SCREEN_CMD);                     /* 列低地址 */
        write_byte(0x10, SCREEN_CMD);                     /* 列高地址 */

        for (col = 0; col < SCREEN_WIDTH; col++) {
            write_byte(s_gram[col][page], SCREEN_DATA);
        }
    }
}

void screen_clear(void)
{
    uint8_t page;
    uint8_t col;

    for (page = 0; page < SCREEN_PAGES; page++) {
        for (col = 0; col < SCREEN_WIDTH; col++) {
            s_gram[col][page] = 0x00;
        }
    }

    screen_refresh();
}

/* ------------------------------------------------------------------ 绘图 */

void screen_char(uint8_t x, uint8_t y, char ch)
{
    uint8_t row;
    uint8_t bit;
    uint8_t pattern;
    uint8_t y0 = y;

    if (ch < ' ' || ch > '~') {
        return;
    }

    ch = (char)(ch - ' ');

    for (row = 0; row < 12; row++) {
        pattern = font_ascii_12[(uint8_t)ch][row];

        for (bit = 0; bit < 8; bit++) {
            if (pattern & 0x80) {
                set_pixel(x, y, 1);
            } else {
                set_pixel(x, y, 0);
            }
            pattern <<= 1;
            y++;

            if ((y - y0) == 12) {
                y = y0;
                x++;
                break;
            }
        }
    }
}

void screen_text(uint8_t x, uint8_t y, const char *text)
{
    while (*text != '\0') {
        if (x > TEXT_MAX_X) {
            x = 0;
            y += 16;
        }
        if (y > TEXT_MAX_Y) {
            y = 0;
            x = 0;
            screen_clear();
        }

        screen_char(x, y, *text);

        x += CHAR_PITCH;
        text++;
    }
}

void screen_number(uint8_t x, uint8_t y, uint32_t number, uint8_t digits)
{
    uint8_t i;
    uint8_t digit;
    uint8_t started = 0;
    uint32_t weight;

    for (i = 0; i < digits; i++) {
        /* 取第 i 位（从左往右） */
        weight = 1;
        {
            uint8_t k;
            for (k = 0; k < (uint8_t)(digits - i - 1); k++) {
                weight *= 10u;
            }
        }
        digit = (uint8_t)((number / weight) % 10u);

        if (!started && i < (uint8_t)(digits - 1)) {
            if (digit == 0) {
                screen_char((uint8_t)(x + CHAR_WIDTH * i), y, ' ');   /* 高位补空 */
                continue;
            }
            started = 1;
        }

        screen_char((uint8_t)(x + CHAR_WIDTH * i), y, (char)('0' + digit));
    }
}

void screen_float(uint8_t x, uint8_t y, float value, uint8_t int_len, uint8_t dec_len)
{
    int   int_part;
    int   frac_part;
    float abs_value = value;

    if (abs_value < 0.0f) {
        abs_value = -abs_value;
        screen_char(x, y, '-');
    } else {
        screen_char(x, y, '+');
    }

    int_part  = (int)abs_value;
    frac_part = (int)((abs_value - (float)int_part) * 1000.0f);

    screen_number((uint8_t)(x + 8), y, (uint32_t)int_part, int_len);
    screen_char((uint8_t)(x + 6 + 8 * int_len), y, '.');

    if (frac_part < 100) {
        if (dec_len == 3) {
            screen_number((uint8_t)(x + 12 + 8 * int_len), y, 0, 1);
            if (frac_part >= 10) {
                screen_number((uint8_t)(x + 18 + 8 * int_len), y, (uint32_t)frac_part, 2);
            } else {
                screen_number((uint8_t)(x + 18 + 8 * int_len), y, 0, 1);
                screen_number((uint8_t)(x + 24 + 8 * int_len), y, (uint32_t)frac_part, 1);
            }
        } else {
            screen_number((uint8_t)(x + 12 + 8 * int_len), y, 0, 1);
            screen_number((uint8_t)(x + 18 + 8 * int_len), y, (uint32_t)(frac_part / 10), 1);
        }
    } else {
        if (dec_len == 3) {
            screen_number((uint8_t)(x + 12 + 8 * int_len), y, (uint32_t)frac_part, 3);
        } else {
            screen_number((uint8_t)(x + 12 + 8 * int_len), y, (uint32_t)(frac_part / 10), 2);
        }
    }
}

/* ------------------------------------------------------------------ 初始化 */

void screen_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(SCREEN_GPIO_CLK, ENABLE);

    gpio.GPIO_Pin   = SCREEN_RST_PIN | SCREEN_DC_PIN | SCREEN_SCL_PIN | SCREEN_SDA_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(SCREEN_GPIO_PORT, &gpio);

    SCREEN_RST_LOW();
    delay_ms(100);
    SCREEN_RST_HIGH();

    write_byte(0xAE, SCREEN_CMD);   /* 关显示 */
    write_byte(0xD5, SCREEN_CMD);   /* 时钟分频 / 振荡频率 */
    write_byte(0x50, SCREEN_CMD);
    write_byte(0xA8, SCREEN_CMD);   /* 复用率 */
    write_byte(0x3F, SCREEN_CMD);
    write_byte(0xD3, SCREEN_CMD);   /* 显示偏移 */
    write_byte(0x00, SCREEN_CMD);
    write_byte(0x40, SCREEN_CMD);   /* 起始行 */
    write_byte(0x8D, SCREEN_CMD);   /* 电荷泵 */
    write_byte(0x14, SCREEN_CMD);
    write_byte(0x20, SCREEN_CMD);   /* 寻址模式：页模式 */
    write_byte(0x02, SCREEN_CMD);
    write_byte(0xA1, SCREEN_CMD);   /* 段重映射 */
    write_byte(0xC0, SCREEN_CMD);   /* COM 扫描方向 */
    write_byte(0xDA, SCREEN_CMD);   /* COM 引脚配置 */
    write_byte(0x12, SCREEN_CMD);
    write_byte(0x81, SCREEN_CMD);   /* 对比度 */
    write_byte(0xEF, SCREEN_CMD);
    write_byte(0xD9, SCREEN_CMD);   /* 预充电周期 */
    write_byte(0xF1, SCREEN_CMD);
    write_byte(0xDB, SCREEN_CMD);   /* VCOMH 电压 */
    write_byte(0x30, SCREEN_CMD);
    write_byte(0xA4, SCREEN_CMD);   /* 全局显示跟随 GRAM */
    write_byte(0xA6, SCREEN_CMD);   /* 正常显示（非反白） */
    write_byte(0xAF, SCREEN_CMD);   /* 开显示 */

    screen_clear();
}
