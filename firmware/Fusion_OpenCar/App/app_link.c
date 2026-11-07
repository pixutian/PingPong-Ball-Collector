/*
 * app_link.c
 * -----------------------------------------------------------------------------
 * 蓝牙通讯：接收中断、模块应答过滤、指令解析、数据回传。
 *
 * 接收放在 USART2 中断里。每个字节先过一遍"模块应答过滤"：蓝牙模块在配对
 * 和断开时会主动往串口吐提示文本，这些字节不能当成摇杆指令，必须在解析
 * 之前丢弃，否则会被误判成某个方向。
 */

#include "app_link.h"
#include "app_config.h"
#include "app_chassis.h"
#include "board.h"
#include "serial.h"
#include "clock.h"

#include <math.h>
#include <string.h>

volatile uint8_t  g_link_dir            = 0;
volatile uint8_t  g_link_page           = 0;
volatile uint8_t  g_link_save_request   = 0;
volatile uint8_t  g_link_report_request = 0;
uint16_t          g_link_param1         = 100;   /* 上电默认值，可在 APP 上改 */
uint16_t          g_link_param2         = 400;

static uint32_t s_gear_key_tick = 0;    /* 底盘档位键上一次生效的时间 */
static uint32_t s_aux_key_tick  = 0;    /* 叶片电机档位键上一次生效的时间 */

/* ---------------------------------------------------------- 模块应答过滤 */

/*
 * 三条特征串同时挂在一个状态机上，靠"上一字节 + 当前字节"判断入口：
 *   SPP 连接 : "+CONNECTING<<XX:XX:XX:XX:XX:XX\r\nCONNECTED\r\r\n"
 *   BLE 连接 : "CONNECTED\r\r\n"
 *   断开     : "+DISC:SUCCESS\r\r\n\0"
 * 进入分支后逐字节比对，比对成功就一直过滤到特征串结束；中途不匹配就退回
 * 空闲态，把该字节放行。
 */
static uint8_t module_noise_filter(uint8_t recv)
{
    static uint8_t last_recv   = 0;
    static uint8_t filter_idx  = 0;
    static uint8_t state       = 0;

    const char *spp_connect  = "+CONNECTING<<XX:XX:XX:XX:XX:XX\r\nCONNECTED\r\r\n";
    const char *ble_connect  = "CONNECTED\r\r\n";
    const char *disconnect   = "+DISC:SUCCESS\r\r\n";     /* 末尾还有一个 '\0'，长度需 +1 */

    uint8_t filtered = 0;

    enum {
        NOISE_IDLE = 0,
        NOISE_SPP,
        NOISE_BLE,
        NOISE_DISC
    };

    switch (state) {
        case NOISE_IDLE:
            if (recv == 'C' && last_recv == '+') {
                state      = NOISE_SPP;
                filtered   = 1;
                filter_idx = 2;
            } else if (recv == 'O' && last_recv == 'C') {
                state      = NOISE_BLE;
                filtered   = 1;
                filter_idx = 2;
            } else if (recv == 'D' && last_recv == '+') {
                state      = NOISE_DISC;
                filtered   = 1;
                filter_idx = 2;
            }
            break;

        case NOISE_SPP:
            if (spp_connect[filter_idx] == (char)recv) {
                filtered = 1;
            } else if ((filter_idx >= 13) && (filter_idx <= 29) &&
                       (((recv >= '0') && (recv <= '9')) || ((recv >= 'A') && (recv <= 'Z')))) {
                filtered = 1;                 /* MAC 地址段，逐字节吃掉 */
            } else {
                state = NOISE_IDLE;           /* 不匹配，放行并退出过滤 */
            }

            filter_idx++;
            if (filter_idx >= strlen(spp_connect)) {
                state = NOISE_IDLE;
            }
            break;

        case NOISE_BLE:
            if (ble_connect[filter_idx] == (char)recv) {
                filtered = 1;
            } else {
                state = NOISE_IDLE;
            }

            filter_idx++;
            if (filter_idx >= strlen(ble_connect)) {
                state = NOISE_IDLE;
            }
            break;

        case NOISE_DISC:
            if (disconnect[filter_idx] == (char)recv) {
                filtered = 1;
            } else {
                state = NOISE_IDLE;
            }

            filter_idx++;
            if (filter_idx >= (strlen(disconnect) + 1)) {
                state = NOISE_IDLE;
            }
            break;

        default:
            state = NOISE_IDLE;
            break;
    }

    last_recv = recv;

    return filtered;
}

/* --------------------------------------------------------------- 帧解析 */

/*
 * APP 参数帧：{? ... } ，帧内字节原样缓存在 param_frame 里（'{' 也算第 0 个字节）。
 *   "{Q:P}"            请求回传当前参数
 *   "{Q:W}"            请求保存参数
 *   "{0:123}" 等       单通道赋值
 *   "{#a:b:c...}"      一次性下发多个参数
 */
static void parse_parameter_frame(uint8_t *param_frame, uint8_t frame_len)
{
    int      j;
    float    value = 0.0f;
    uint16_t parsed;

    if (param_frame[3] == 'P') {
        g_link_report_request = 1;
        return;
    }

    if (param_frame[3] == 'W') {
        g_link_save_request = 1;
        return;
    }

    if (param_frame[1] != '#') {
        /* 冒号后面是十进制数字，从最低位往高位累加 */
        for (j = (int)frame_len; j >= 4; j--) {
            value += (float)(param_frame[j - 1] - '0') *
                     (float)pow(10.0, (double)((int)frame_len - j));
        }
        parsed = (uint16_t)value;

        switch (param_frame[1]) {
            case '0': g_link_param1 = parsed; break;
            case '1': g_link_param2 = parsed; break;
            default:  break;
        }
        return;
    }

    /* "#" 开头的多参数帧：按冒号切分，取前两个值 */
    {
        uint8_t idx = 0;
        uint8_t slot = 0;
        float   accum = 0.0f;
        float   values[9] = {0.0f};

        for (idx = 0; idx < frame_len && idx < 9; idx++) {
            if (param_frame[idx] >= '0' && param_frame[idx] <= '9') {
                accum = accum * 10.0f + (float)(param_frame[idx] - '0');
            } else if (param_frame[idx] == ':') {
                values[slot++] = accum;
                accum = 0.0f;
            }
        }
        values[slot] = accum;          /* 最后一个字段 */

        g_link_param1 = (uint16_t)values[0];
        g_link_param2 = (uint16_t)values[1];
    }
}

/* --------------------------------------------------------------- 解码 */

static void link_decode(uint8_t recv)
{
    static uint8_t param_flag = 0;
    static uint8_t param_count = 0;
    static uint8_t param_frame[50] = {0};

    /* 模块自己的提示文本先丢掉 */
    if (module_noise_filter(recv)) {
        return;
    }

    /* 页面切换 */
    if (recv == 'K') {
        g_link_page = 2;
    } else if (recv == 'J') {
        g_link_page = 1;
    } else if (recv == 'I') {
        g_link_page = 0;
    }

    /*
     * 档位键：X 升档 / Y 降档。
     * 按住时 APP 会连续重发同一个字节，这里用时间窗去抖，一次按下只调一档；
     * 调完直接返回，不改写方向指令，行进方向与新速度互不干扰。
     */
    if (recv == 'X' || recv == 'Y') {
        uint32_t now = tick_ms();

        if ((s_gear_key_tick == 0) ||
            ((uint32_t)(now - s_gear_key_tick) >= GEAR_KEY_DEBOUNCE_MS)) {
            s_gear_key_tick = now;
            chassis_gear_change((recv == 'X') ? 1 : -1);
        }
        return;
    }

    /*
     * 叶片电机档位键：T 升档 / U 降档，7 档循环，每档 10% 占空比。
     * 与 X/Y 同理：按住时 APP 会连发同一字节，必须用时间窗去抖，
     * 否则会在几毫秒里连跳好几档。处理完直接返回，不影响行进方向。
     */
    if (recv == 'T' || recv == 'U') {
        uint32_t now = tick_ms();

        if ((s_aux_key_tick == 0) ||
            ((uint32_t)(now - s_aux_key_tick) >= GEAR_KEY_DEBOUNCE_MS)) {
            s_aux_key_tick = now;
            aux_gear_change((recv == 'T') ? 1 : -1);
        }
        return;
    }

    /* 方向键：0x41~0x48 对应 1~8 */
    g_link_dir = (uint8_t)(recv - 0x40);

    /* 帧收集与解析 */
    if (recv == '{') {
        param_flag = 1;
    } else if (recv == '}') {
        param_flag = 2;
    }

    if (param_flag == 1) {
        param_frame[param_count % sizeof(param_frame)] = recv;
        param_count++;
    } else if (param_flag == 2) {
        parse_parameter_frame(param_frame, param_count);

        param_flag  = 0;
        param_count = 0;
        memset(param_frame, 0, sizeof(param_frame));
    }
}

/* --------------------------------------------------------------- 中断入口 */

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(LINK_UART, USART_IT_RXNE) != RESET) {
        uint8_t byte = (uint8_t)USART_ReceiveData(LINK_UART);

        link_decode(byte);
        USART_ClearITPendingBit(LINK_UART, USART_IT_RXNE);
    }
}

/* --------------------------------------------------------------- 对外接口 */

void link_init(uint32_t baud)
{
    link_uart_init(baud);

    g_link_dir            = 0;
    g_link_page           = 0;
    g_link_save_request   = 0;
    g_link_report_request = 0;
}

void link_send_text(const char *text)
{
    link_uart_send(text);
}
